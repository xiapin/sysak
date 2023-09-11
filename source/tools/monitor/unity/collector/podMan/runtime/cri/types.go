package main

import (
	pb "k8s.io/cri-api/pkg/apis/runtime/v1"
)

type ContainerInfo struct {
	Id              string
	PodName         string
	PodId           string
	Pid             int
	Namespace       string
	PodCgroup       string
	ContainerCgroup string
	ContainerName   string
}

type PodInfo struct {
	CgroupParent string
}

// Container is an interface for get the container id and name
type Container struct {
	Id           string
	Names        []string
	State        string
	PodSandboxId string
}

type CRIContainerStatus struct {
	Info   *CRIContainerInfo `json:"info"`
	Status *pb.ContainerStatus
}

type CRIContainerInfo struct {
	Pid            int64               `json:"pid"`
	Config         *pb.ContainerConfig `json:"config"`
	RuntimeOptions struct {
		SystemdCgroup bool `json:"systemd_cgroup"`
	} `json:"runtimeOptions"`
	RuntimeSpec struct {
		Linux struct {
			CgroupsPath string `json:"cgroupsPath"`
		} `json:"linux"`
	} `json:"RuntimeSpec"`
}

type CRIPodStatus struct {
	Info   *CRIPodInfo `json:"info"`
	Status *pb.PodSandboxStatus
}

type CRIPodInfo struct {
	Config *pb.PodSandboxConfig `json:"config"`
}
