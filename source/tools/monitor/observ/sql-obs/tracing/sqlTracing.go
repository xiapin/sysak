package tracing

import (
	"errors"
	"fmt"
	"net"
	"os"
)

func notifyMysqlInstancesInfo(pidList string) error {
	if _, err := os.Stat("/var/ntopo"); os.IsNotExist(err) {
		return err
	}
	addr, err := net.ResolveUnixAddr("unix", "/var/ntopo")
	if err != nil {
		return err
	}
	sock, err := net.DialUnix("unix", nil, addr)
	// gCnfPut.sock, err = net.Dial("unixgram", path)
	if err != nil {
		return err
	}

	if len(pidList) > MAX_BUFF {
		return errors.New("pidList too long")
	}

	if _, err := sock.Write([]byte(pidList)); err != nil {
		//fmt.Println(s)
		fmt.Printf("output: %s\n", pidList)
		return err
	}

	return nil
}
