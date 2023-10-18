package main

import (
	"context"
	"encoding/json"
	"fmt"
	"net"
	"net/url"
	"time"

	pb "k8s.io/cri-api/pkg/apis/runtime/v1"

	"github.com/pkg/errors"
	"google.golang.org/grpc"
	"k8s.io/klog/v2"
)

const (
	// unixProtocol is the network protocol of unix socket.
	unixProtocol = "unix"
	Timeout      = 1 * time.Second
)

// GetAddressAndDialer returns the address parsed from the given endpoint and a context dialer.
func GetAddressAndDialer(endpoint string) (string, func(ctx context.Context, addr string) (net.Conn, error), error) {
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
			klog.InfoS("Using this format as endpoint is deprecated, please consider using full url format.", "deprecatedFormat", endpoint, "fullURLFormat", fallbackEndpoint)
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

func getRuntimeClient(endpoints []string) (pb.RuntimeServiceClient, *grpc.ClientConn, error) {
	// Set up a connection to the server.
	conn, err := getConnection(endpoints)
	if err != nil {
		return nil, nil, errors.Wrap(err, "connect")
	}
	runtimeClient := pb.NewRuntimeServiceClient(conn)
	return runtimeClient, conn, nil
}

func getConnection(endPoints []string) (*grpc.ClientConn, error) {
	if endPoints == nil || len(endPoints) == 0 {
		return nil, fmt.Errorf("endpoint is not set")
	}
	endPointsLen := len(endPoints)
	var conn *grpc.ClientConn
	for indx, endPoint := range endPoints {
		//klog.Infof("connect using endpoint '%s' with '%s' timeout", endPoint, Timeout)
		addr, dialer, err := GetAddressAndDialer(endPoint)
		if err != nil {
			if indx == endPointsLen-1 {
				return nil, err
			}
			klog.Warningf("%v", err)
			continue
		}
		conn, err = grpc.Dial(addr, grpc.WithInsecure(), grpc.WithBlock(), grpc.WithTimeout(Timeout), grpc.WithContextDialer(dialer))
		if err != nil {
			errMsg := errors.Wrapf(err, "connect endpoint '%s', make sure you are running as root and the endpoint has been started", endPoint)
			if indx == endPointsLen-1 {
				return nil, errMsg
			}
			klog.Warningf("%v", errMsg)
		} else {
			//klog.Infof("connected successfully using endpoint: %s", endPoint)
			break
		}
	}
	return conn, nil
}

func ParseContainerStatusResponse(response *pb.ContainerStatusResponse) (CRIContainerStatus, error) {
	containerStatus := CRIContainerStatus{
		Status: response.Status,
	}
	data, found := response.GetInfo()["info"]
	if !found {
		return containerStatus, fmt.Errorf("not found field 'info' in container status response")
	}
	info := &CRIContainerInfo{}
	if err := json.Unmarshal([]byte(data), info); err != nil {
		return containerStatus, err
	}
	containerStatus.Info = info
	return containerStatus, nil
}

func ParsePodStatusResponse(response *pb.PodSandboxStatusResponse) (CRIPodStatus, error) {
	podStatus := CRIPodStatus{
		Status: response.Status,
	}
	data, found := response.GetInfo()["info"]
	if !found {
		return podStatus, fmt.Errorf("not found field 'info' in pod status response")
	}
	info := &CRIPodInfo{}
	if err := json.Unmarshal([]byte(data), info); err != nil {
		return podStatus, err
	}
	podStatus.Info = info
	return podStatus, nil
}
