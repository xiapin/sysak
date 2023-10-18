package common

import (
    // "crypto/rand"
    // "crypto/rsa"
    // "crypto/x509"
    // "encoding/pem"
    "unsafe"
    // "flag"
    "fmt"
    "os"
)

func usage() {
    fmt.Printf("Usage: %s [options]\n", os.Args[0])
    fmt.Printf("Options:\n")
    fmt.Printf("  u       The DB monitor username\n")
    fmt.Printf("  p       The DB monitor password\n")
    fmt.Printf("  s       Encryption type of account information(BASE,RSA)\n")
    fmt.Printf("  y       Specify the yaml file(default is /etc/sysak/base.yaml)\n")
    os.Exit(0) 
}

func GetRawUsersInfo() (string, string, string) {
    usersInfo := getArgsFromCmdline([]string{"-u", "-p", "-s"}, true)
    return usersInfo[0], usersInfo[1], usersInfo[2]
}

func GetYamlFile() (string) {
    yaml := getArgsFromCmdline([]string{"-y"}, false)[0]
    if yaml == "" {
        return "/etc/sysak/base.yaml"
    }
    return yaml
}

func getArgsFromCmdline(opts []string, hide bool) []string {
    args := os.Args[1:]
    retArgs := make([]string, len(opts))
    for idx, val := range opts {
        if val == "-h" || val == "--help" {
            usage()
        }
        retArgs[idx] = ""
        for i := 0; i < len(args); i++ {
            if args[i] == val && len(args) > (i + 1) {
                retArgs[idx] = string([]byte(args[i + 1]))
                if hide {
                    hideParam(i + 2)
                }
                break
            }
        }
    }
    return retArgs
}

func hideParam(index int) {
    p := *(*unsafe.Pointer)(unsafe.Pointer(&os.Args[index]))
    for i := 0; i < len(os.Args[index]); i++ {
        *(*uint8)(unsafe.Pointer(uintptr(p) + uintptr(i))) = 'x'
    }
}