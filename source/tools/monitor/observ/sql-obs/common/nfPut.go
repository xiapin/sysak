package common

import (
    "encoding/json"
    //"fmt"
    "net"
    "os"
    "strings"
)

const (
    //PIPE_PATH string = "/var/sysom/outline" // 参考 YAML 中的配置
    MAX_BUFF    int = 64 * 1024 // 最大消息长度
    TIME_FORMAT     = "2006-01-02 15:04:05"
)

type CnfPut struct {
    sock *net.UnixConn
    // sock net.Conn
    path string
}

var gCnfPut CnfPut

func newCnfPut(path string) error {
    gCnfPut.path = path
    if _, err := os.Stat(gCnfPut.path); os.IsNotExist(err) {
        PrintSysError(err)
        return err
    }
    addr, err := net.ResolveUnixAddr("unixgram", gCnfPut.path)
    if err != nil {
        PrintSysError(err)
        return err
    }
    gCnfPut.sock, err = net.DialUnix("unixgram", nil, addr)
    if err != nil {
        PrintSysError(err)
        return err
    }
    return nil
}

func (c *CnfPut) puts(s string) error {
    if len(s) > MAX_BUFF {
        return PrintOnlyErrMsg(
            "message len %d, is too long, should less than %d, data :\n%s",
            len(s), MAX_BUFF, s)
    }

    if _, err := c.sock.Write([]byte(s)); err != nil {
        PrintSysError(err)
        return err
    }
    return nil
}

func ExportData(s string) error {
    if err := gCnfPut.puts(s); err != nil {
        return err
    }
    return nil
}

func Struct2String(s any) string {
    b, err := json.Marshal(s)
    if err != nil {
        return ""
    }
    t_s := string(b)
    t_s = strings.ReplaceAll(t_s, ":", "=")
    t_s = strings.ReplaceAll(t_s, `"`, "")
    //fmt.Println(t_s)
    return t_s[1 : len(t_s)-1]
}

func InitDataExport(path string) error {
    err := newCnfPut(path)
    if err != nil {
        return err
    }
    return nil
}
