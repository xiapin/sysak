package main

import (
	"C"
	"encoding/json"
	"fmt"
)

func GetContainerInfos(endpoints []string) ([]ContainerInfo, error) {
	c, err := newCri(endpoints)
	if err != nil {
		return nil, err
	}

	defer c.Shutdown()
	return c.GetContainerInfos()
}

//export CGetContainerInfosfunc
func CGetContainerInfosfunc(endpoint *C.char) *C.char {
	ep := C.GoString(endpoint)
	endpoints := []string{ep}
	//fmt.Println(endpoints)
	infos, err := GetContainerInfos(endpoints)
	if err != nil {
		fmt.Printf("cri: get container info failed: %v", err)
		return nil
	}
	marshal, _ := json.Marshal(infos)
	//fmt.Println(string(marshal))
	//return string(marshal)
	return C.CString(string(marshal))
	//return "hello"
}

//export CheckRuntime
func CheckRuntime(endpoint_ptr *string) int {
	endpoints := []string{*endpoint_ptr}
	//fmt.Println(endpoints)
	c, err := newCri(endpoints)
	defer c.Shutdown()
	if err != nil {
		fmt.Printf("failed to connect to containerd: %v", err)
		return 0
	}
	return 1
}

func main() {
	var containerdEndpoints = []string{
		"unix:///mnt/host/run/containerd/containerd.sock",
		"unix:///mnt/host/var/run/containerd/containerd.sock",
		"unix:///mnt/host/run/containerd.sock",
	}

	for _, ep := range containerdEndpoints {
		if CheckRuntime(&ep) == 0 {
			return
		}
	}

	infos, _ := GetContainerInfos(containerdEndpoints)
	marshal, _ := json.Marshal(infos)
	fmt.Println(string(marshal))
}
