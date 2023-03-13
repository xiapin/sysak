package util

import (
	"bufio"
	"encoding/binary"
	"fmt"
	"os"
	"regexp"
	"strconv"
	"strings"
)

var (
	/*
		12:pids:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-podb617b9b7_70aa_4571_9bdd_d5b079bb1510.slice/cri-containerd-220720d168658afcd6271f67ab39b4a36ef9dd194be9da684613b39e36066c75.scope
	*/
	//kubePattern   = regexp.MustCompile(`\d+:.+:/kubepods/[^/]+/pod[^/]+/([0-9a-f]{64})`)
	kubePattern   = regexp.MustCompile(`\d+:.+:/kubepods.+pod.+-([0-9a-f]{64}).scope`)
	dockerPattern = regexp.MustCompile(`\d+:.+:/docker/pod[^/]+/([0-9a-f]{64})`)
)

func NetToHostShort(i uint16) uint16 {
	data := make([]byte, 2)
	binary.BigEndian.PutUint16(data, i)
	return binary.LittleEndian.Uint16(data)
}

func NetToHostLong(i uint32) uint32 {
	data := make([]byte, 4)
	binary.BigEndian.PutUint32(data, i)
	return binary.LittleEndian.Uint32(data)
}

func HostToNetShort(i uint16) uint16 {
	b := make([]byte, 2)
	binary.LittleEndian.PutUint16(b, i)
	return binary.BigEndian.Uint16(b)
}

func HostToNetLong(i uint32) uint32 {
	b := make([]byte, 4)
	binary.LittleEndian.PutUint32(b, i)
	return binary.BigEndian.Uint32(b)
}

func Ipv4ToInt(s string) int {
	arr := strings.Split(s, ".")
	res := 0
	for i, str := range arr {
		val, _ := strconv.Atoi(str)
		val = val << (24 - 8*i)
		res |= val
	}
	return res
}

func IntToIpv4(ip int) string {
	return fmt.Sprintf("%d.%d.%d.%d",
		byte(ip>>24), byte(ip>>16), byte(ip>>8), byte(ip))
}

func LookupContainerId(pid string) (string, error) {
	f, err := os.Open(fmt.Sprintf("/proc/%s/cgroup", pid))
	if err != nil {
		return "", nil
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		line := scanner.Text()
		parts := kubePattern.FindStringSubmatch(line)
		if parts != nil {
			// Get 12 bytes correspond to k8s containerId
			return fmt.Sprintf("%s", parts[1][0:12]), nil
		}
	}
	return "", nil
}

func isNum(s string) bool {
	_, err := strconv.ParseFloat(s, 64)
	return err == nil
}

func LookupAllPids() ([]string, error) {
	pids := make([]string, 0)
	items, err := os.ReadDir("/proc")
	if err != nil {
		fmt.Printf("Read proc failed:%v", err)
		return nil, err
	}
	for _, entry := range items {
		if isNum(entry.Name()) {
			pids = append(pids, entry.Name())
		}
	}
	return pids, nil
}

/*
func main() {
	path := "12:pids:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-podb617b9b7_70aa_4571_9bdd_d5b079bb1510.slice/cri-containerd-220720d168658afcd6271f67ab38b4a36ef9dd194be9da684613b39e36066c75.scope"
	name := kubePattern.FindStringSubmatch(path)
	fmt.Printf("Container id:%s", name[1][0:12])
	// Container id:220720d16865
}
*/
