package main

import (
	"context"
	"fmt"
	"strings"

	pb "k8s.io/cri-api/pkg/apis/runtime/v1"

	"google.golang.org/grpc"
	"k8s.io/klog/v2"
)

type Cri struct {
	enabled       bool
	ctx           context.Context
	runtimeClient pb.RuntimeServiceClient
	runtimeConn   *grpc.ClientConn
}

/*
var containerdEndpoints = []string{
	"unix:///mnt/host/run/containerd/containerd.sock",
	"unix:///mnt/host/var/run/containerd/containerd.sock",
	"unix:///mnt/host/run/containerd.sock",
}
*/

func newCri(endpoints []string) (Cri, error) {
	c := Cri{}
	c.enabled = false
	runtimeClient, runtimeConn, err := getRuntimeClient(endpoints)
	if err != nil {
		//klog.Warningf("failed to create runtime client, %v", err)
		return c, err
	}
	c.enabled = true
	c.runtimeClient = runtimeClient
	c.runtimeConn = runtimeConn
	c.ctx = context.Background()
	return c, nil
}

func (c *Cri) GetContainerInfos() ([]ContainerInfo, error) {
	allContainerInfos := []ContainerInfo{}

	cs, err := c.ListContainers(c.ctx)
	if err != nil {
		klog.Errorf("failed to list container,%v", err)
		return nil, err
	}

	for _, con := range cs {
		containerInfo, err := c.InspectContainer(con, c.ctx)
		if err != nil {
			klog.Errorf("failed to inspect container %v, %v", con.Id, err)
			continue
		}
		klog.V(7).Infof("not found container in cache and succeed to inspect container %v", con.Id)
		klog.V(7).Infof("get container info: %v", con.Id, containerInfo)

		podId := con.PodSandboxId
		podInfo, err := c.InspectPod(podId, c.ctx)
		if err != nil {
			klog.Errorf("failed to inspect Pod%v, %v", podId, err)
		}
		cgroupParent := podInfo.CgroupParent
		containerInfo.PodCgroup = cgroupParent
		containerInfo.ContainerCgroup = cgroupParent + containerInfo.ContainerCgroup
		allContainerInfos = append(allContainerInfos, containerInfo)
	}

	return allContainerInfos, nil
}

func (c *Cri) ListContainers(ctx context.Context) ([]Container, error) {
	request := &pb.ListContainersRequest{}
	r, err := c.runtimeClient.ListContainers(ctx, request)
	if err != nil {
		return nil, fmt.Errorf("failed to list containers, %v", err)
	}
	containers := []Container{}
	for _, con := range r.Containers {
		state := con.GetState()
		klog.V(7).Infof("container name: %v, status: %v", con.Metadata.Name, state.String())
		// make sure we only need running containers
		if state == pb.ContainerState_CONTAINER_CREATED || state == pb.ContainerState_CONTAINER_EXITED {
			continue
		}
		containers = append(containers, Container{
			Id:           con.Id,
			Names:        []string{con.Metadata.Name},
			State:        con.State.String(),
			PodSandboxId: con.PodSandboxId,
		})
	}
	return containers, nil
}

func (c *Cri) InspectContainer(container Container, ctx context.Context) (ContainerInfo, error) {
	var containerCgroup string

	containerInfo := ContainerInfo{}
	request := &pb.ContainerStatusRequest{
		ContainerId: container.Id,
		Verbose:     true,
	}
	r, err := c.runtimeClient.ContainerStatus(context.Background(), request)
	if err != nil {
		return containerInfo, fmt.Errorf("failed to inspect container %v,reason: %v", container.Id, err)
	}
	klog.V(7).Infof("succeed to get container status response for container %v", container.Id)
	klog.V(7).Infof("result of containerStatus.GetInfo()", r.GetInfo())
	containerStatus, err := ParseContainerStatusResponse(r)
	if err != nil {
		return containerInfo, err
	}
	klog.V(7).Infof("succeed to parse container status response for container %v", container.Id)
	klog.V(7).Infof("container status: %v", containerStatus)

	containerName := containerStatus.Status.Metadata.Name
	if containerStatus.Info.Config.Labels["io.kubernetes.container.name"] != "" {
		containerName = containerStatus.Info.Config.Labels["io.kubernetes.container.name"]
	}
	containerName = strings.Trim(containerName, "/")

	rawCgroupPath := containerStatus.Info.RuntimeSpec.Linux.CgroupsPath
	if containerStatus.Info.RuntimeOptions.SystemdCgroup {
		/*
		 * if systemdCgroup is true, rawCgroupPath would be
		 * "kubepods-burstable-pod$(pod uid).slice:cri-containerd:$(container id)"
		 */
		paths := strings.Split(rawCgroupPath, ":")
		if len(paths) != 3 {
			return containerInfo, fmt.Errorf("failed to parse container cgroup path %v", rawCgroupPath)
		}
		// the final path would be "cri-containerd-$(container id).scope"
		containerCgroup = "/" + paths[1] + "-" + paths[2] + ".scope"
	} else {
		/*
		 * cgroupfs driver, rawCgrouppath would be
		 *  $(container id)
		 */
		containerCgroup = "/" + container.Id
	}

	return ContainerInfo{
		Id:              container.Id,
		PodName:         containerStatus.Info.Config.Labels["io.kubernetes.pod.name"],
		PodId:           containerStatus.Info.Config.Labels["io.kubernetes.pod.uid"],
		ContainerName:   containerName,
		Namespace:       containerStatus.Info.Config.Labels["io.kubernetes.pod.namespace"],
		Pid:             int(containerStatus.Info.Pid),
		ContainerCgroup: containerCgroup,
	}, nil
}

func (c *Cri) InspectPod(podId string, ctx context.Context) (PodInfo, error) {
	podInfo := PodInfo{}
	request := &pb.PodSandboxStatusRequest{
		PodSandboxId: podId,
		Verbose:      true,
	}
	r, err := c.runtimeClient.PodSandboxStatus(context.Background(), request)
	if err != nil {
		return podInfo, fmt.Errorf("failed to inspect pod %v,reason: %v", podId, err)
	}
	klog.V(7).Infof("succeed to get pod status response for container %v", podId)
	klog.V(7).Infof("result of podsandboxStatus.GetInfo()", r.GetInfo())

	podStatus, err := ParsePodStatusResponse(r)
	if err != nil {
		return podInfo, err
	}

	return PodInfo{
		CgroupParent: podStatus.Info.Config.Linux.CgroupParent,
	}, nil
}

func (c *Cri) Shutdown() error {
	if !c.enabled {
		return nil
	}
	if c.runtimeConn != nil {
		return c.runtimeConn.Close()
	}
	return nil
}
