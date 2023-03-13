package k8s

import (
	"context"
	"fmt"
	"log"
	"net"
	"net/url"
	"os"
	"time"
	"sync"

	"github.com/pkg/errors"
	"google.golang.org/grpc"
	"github.com/chentao-kernel/cloud_ebpf/util"
	cri "k8s.io/cri-api/pkg/apis/runtime/v1alpha2"
)

const (
	// unixProtocol is the network protocol of unix socket.
	unixProtocol = "unix"
)

var (
	// RuntimeEndpoint is CRI server runtime endpoint
	RuntimeEndpoint = []string{"unix:///var/run/dockershim.sock", "unix:///run/crio/crio.sock", "unix:///run/containerd/containerd.sock"}
	// Timeout  of connecting to server (default: 10s)
	Timeout = 10 * time.Second
)

type ContainerTupe struct {
	Name string
	Status string
	Id string
	//Pid int
}

type ContainerManager struct {
	ContainerInfoMap map[string]*ContainerTupe
	Client cri.RuntimeServiceClient
	Conn *grpc.ClientConn
	Lock sync.Mutex
	Pid2ContainerMap map[string]string
}

var ContainerManagerPr *ContainerManager

func NewContainerManager()(*ContainerManager, error) {
	client, conn, err := newRuntimeClient()
	if (err != nil) {
		log.Printf("New runtime client failed:%v", err)
		return nil, err
	}
	return &ContainerManager{
		ContainerInfoMap:	make(map[string]*ContainerTupe),
		Client:	client,
		Conn:	conn,
		Pid2ContainerMap:	make(map[string]string),
	}, nil
}

func newConnection(endPoints []string) (*grpc.ClientConn, error) {
	if endPoints == nil || len(endPoints) == 0 {
		return nil, fmt.Errorf("endpoint is not set")
	}
	endPointsLen := len(endPoints)
	var conn *grpc.ClientConn
	for indx, endPoint := range endPoints {
		_, sock, _ := parseEndpoint(endPoint)
		if isExist(sock) == false {
			log.Printf("sock: %s No such file or directory\n", sock)
			continue
		}
		log.Printf("connect using endpoint '%s' with '%s' timeout", endPoint, Timeout)
		addr, dialer, err := newAddressAndDialer(endPoint)
		if err != nil {
			if indx == endPointsLen-1 {
				return nil, err
			}
			log.Fatal(err)
			continue
		}
		conn, err = grpc.Dial(addr, grpc.WithInsecure(), grpc.WithBlock(), grpc.WithTimeout(Timeout), grpc.WithContextDialer(dialer))
		if err != nil {
			errMsg := errors.Wrapf(err, "connect endpoint '%s', make sure you are running as root and the endpoint has been started", endPoint)
			if indx == endPointsLen-1 {
				return nil, errMsg
		}
		} else {
			log.Printf("connected successfully using endpoint: %s", endPoint)
			break
		}
	}
	return conn, nil
}

// newAddressAndDialer returns the address parsed from the given endpoint and a context dialer.
func newAddressAndDialer(endpoint string) (string, func(ctx context.Context, addr string) (net.Conn, error), error) {
	protocol, addr, err := parseEndpointWithFallbackProtocol(endpoint, unixProtocol)
	if err != nil {
		return "", nil, err
	}
	if protocol != unixProtocol {
		return "", nil, fmt.Errorf("only support unix socket endpoint")
	}

	return addr, dial, nil
}

func dial(ctx context.Context, addr string) (net.Conn, error) {
	return (&net.Dialer{}).DialContext(ctx, unixProtocol, addr)
}

func parseEndpointWithFallbackProtocol(endpoint string, fallbackProtocol string) (protocol string, addr string, err error) {
	if protocol, addr, err = parseEndpoint(endpoint); err != nil && protocol == "" {
		fallbackEndpoint := fallbackProtocol + "://" + endpoint
		protocol, addr, err = parseEndpoint(fallbackEndpoint)
		if err == nil {
			log.Printf("Using %q as endpoint is deprecated, please consider using full url format %q.", endpoint, fallbackEndpoint)
		}
	}
	return
}

func parseEndpoint(endpoint string) (string, string, error) {
	u, err := url.Parse(endpoint)
	if err != nil {
		return "", "", err
	}
	switch u.Scheme {
	case "tcp":
		return "tcp", u.Host, nil
	case "unix":
		return "unix", u.Path, nil
	case "":
		return "", "", fmt.Errorf("using %q as endpoint is deprecated, please consider using full url format", endpoint)
	default:
		return u.Scheme, "", fmt.Errorf("protocol %q not supported", u.Scheme)
	}
}

func (cm *ContainerManager)GetPods() (error) {

	request := &cri.ListContainersRequest{}
	listContainers, err := cm.Client.ListContainers(context.Background(), request)
	if err != nil {
		log.Printf("List containers failed:%v\n", err)
		return err
	}

	for _, container := range listContainers.Containers {
		id := substr(container.GetId(), 12)
		/*
		var request = &cri.ContainerStatusRequest{ContainerId: id}
		info, err := runtimeClient.ContainerStatus(context.Background(), request)
		if err != nil {
			fmt.Println(err)
		}
		fmt.Println(info.Status.LogPath)
		*/
		var req = &cri.ContainerStatsRequest{ContainerId: id}
		stat, _ := cm.Client.ContainerStats(context.Background(), req)
		if err != nil {
			log.Printf("Stat container failed:%v\n", err)
			return err
		}
		if container.State.String() != "CONTAINER_RUNNING" {
			continue
		}
		cm.ContainerInfoMap[id] = &ContainerTupe{
			Name: stat.Stats.Attributes.Metadata.Name,
			Status: container.State.String(),
			Id: substr(stat.Stats.Attributes.Id, 12),
		}
		//fmt.Printf("container:%v\n", cm.ContainerInfoMap[id])
		//fmt.Println(stat.Stats.WritableLayer.FsId.Mountpoint)
	}
	return nil
}

func (cm *ContainerManager)CloseClient() error {
	if cm.Conn == nil {
		return nil
	}
	return cm.Conn.Close()
}

func (cm *ContainerManager)CachePid2Container() error {
	pids, err := util.LookupAllPids()
	if err != nil {
		log.Printf("Lookup pids failed:%v", err)
		return err
	}
	for _, pid := range pids {
		containerId, err := util.LookupContainerId(pid)
		if (err != nil) {
			// Not Pod process
			cm.Pid2ContainerMap[pid] = ""
			return err
		} else {
			container, ok := cm.ContainerInfoMap[containerId]
			if ok {
				cm.Pid2ContainerMap[pid] = container.Name
			} else {
				cm.Pid2ContainerMap[pid] = ""
			}
		}
	}
	return nil
}

func (cm *ContainerManager)GetContainerNameWithPid(pid string) string {
	name, ok := cm.Pid2ContainerMap[pid]
	if ok {
		return name
	}
	return ""
}

func (cm *ContainerManager)GetContainerNameWithId(containerId string) string {
	value, ok := cm.ContainerInfoMap[containerId]
	if ok {
		return value.Name
	}
	return ""
}

func KubernetsInit() error {
	var err error
	ContainerManagerPr, err = NewContainerManager()
	if (err != nil) {
		return err
	}
	ContainerManagerPr.GetPods()
	return nil
}

func newRuntimeClient() (cri.RuntimeServiceClient, *grpc.ClientConn, error) {
	// Set up a connection to the server.
	conn, err := newConnection(RuntimeEndpoint)
	if err != nil {
		return nil, nil, errors.Wrap(err, "connect")
	}
	runtimeClient := cri.NewRuntimeServiceClient(conn)
	return runtimeClient, conn, nil
}

func closeConnection(conn *grpc.ClientConn) error {
	if conn == nil {
		return nil
	}
	return conn.Close()
}

func substr(s string, l int) string {
	if len(s) <= l {
		return s
	}
	ss, sl, rl, rs := "", 0, 0, []rune(s)
	for _, r := range rs {
		rint := int(r)
		if rint < 128 {
			rl = 1
		} else {
			rl = 2
		}
		if sl+rl > l {
			break
		}
		sl += rl
		ss += string(r)
	}
	return ss
}

func isExist(fileName string) bool {
	_, err := os.Stat(fileName)
	if err != nil {
		if os.IsExist(err) {
			return true
		}
		return false
	}
	return true
}