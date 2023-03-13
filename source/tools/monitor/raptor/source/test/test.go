package main

import (
	"fmt"
	"github.com/chentao-kernel/cloud_ebpf/k8s"
	"github.com/chentao-kernel/cloud_ebpf/util"
)

/*
id:bf9ca3746bc8, container info:&{external-nas-provisioner CONTAINER_RUNNING bf9ca3746bc8}
id:49f57053d785, container info:&{disk-driver-registrar CONTAINER_RUNNING 49f57053d785}
id:be1db5a69dd8, container info:&{node-exporter CONTAINER_RUNNING be1db5a69dd8}
id:a09168dc7f35, container info:&{external-disk-attacher CONTAINER_RUNNING a09168dc7f35}
id:362ddc42b8b2, container info:&{external-snapshot-controller CONTAINER_RUNNING 362ddc42b8b2}
id:0617de752a6a, container info:&{external-csi-snapshotter CONTAINER_RUNNING 0617de752a6a}
id:de9e42bc3fb0, container info:&{external-oss-provisioner CONTAINER_RUNNING de9e42bc3fb0}
id:0c9b2d999a7e, container info:&{policy CONTAINER_RUNNING 0c9b2d999a7e}
id:6a4484f54dd0, container info:&{ack-node-problem-detector CONTAINER_RUNNING 6a4484f54dd0}
id:361a08330614, container info:&{csi-provisioner CONTAINER_RUNNING 361a08330614}
*/
func k8s_test() {
	k8s.KubernetsInit()
	for key, value := range k8s.ContainerManagerPr.ContainerInfoMap {
		fmt.Printf("id:%v, container info:%v\n", key, value)
	}
}

func util_test() {
	pids, err := util.LookupAllPids()
	if err != nil {
		return
	}
	for _, pid := range pids {
		fmt.Printf("pid:%v\n", pid)
	}
}

func main() {
	util_test()
	k8s_test()
}
