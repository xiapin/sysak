package common

import (
	// "crypto/rand"
	// "crypto/rsa"
	// "crypto/x509"
	// "encoding/pem"
	"encoding/base64"
	"unsafe"
	// "flag"
	"fmt"
	"os"
)

func GetUsersInfo() (string, string) {
	secretType := ""
	username := ""
	password := ""

	args := os.Args[1:]
	for i := 0; i < len(args); i++ {
		switch args[i] {
			case "-p":
				password = string([]byte(args[i+1]))
			case "-u":
				username = string([]byte(args[i+1]))
			case "-s":
				secretType = string([]byte(args[i+1]))
			case "-h":
				fmt.Printf("Usage: %s [options]\n", os.Args[0])
				fmt.Printf("Options:\n")
				fmt.Printf("  u       The DB monitor username\n")
				fmt.Printf("  p       The DB monitor password\n")
				fmt.Printf("  s       Encryption type of account information(BASE,RSA)\n")
				os.Exit(0) 
			default:
				hideParam(i + 1)
				continue
		}
	}
	if secretType != "" {
		username = decryptData(username, secretType)
		password = decryptData(password, secretType)
	}
	return username, password
}

func hideParam(index int) {
	p := *(*unsafe.Pointer)(unsafe.Pointer(&os.Args[index]))
	for i := 0; i < len(os.Args[index]); i++ {
		*(*uint8)(unsafe.Pointer(uintptr(p) + uintptr(i))) = 'x'
	}
}

func decryptData(data string, secretType string) string {
	if secretType == "RSA" {
		return decryptRsa([]byte(data))
	} else if secretType == "BASE" {
		return decryptBase64(data)
	}
	return ""
}

func decryptBase64(data string) string {
	decodedBytes, err := base64.StdEncoding.DecodeString(data)
	if err != nil {
		PrintOnlyErrMsg("Failed to decode Base64 string: %s", data)
		return ""
	}
	return string(decodedBytes)
}

func decryptRsa(data []byte) string {
	// decryptedData, err := rsa.DecryptPKCS1v15(rand.Reader, decodeSecretKey(), data)
	// if err != nil {
	// 	PrintOnlyErrMsg("Failed to decrypt data: %s", data)
	// 	return ""
	// }
	// return string(decryptedData)
	return string(data)
}

// func decodeSecretKey() *rsa.PrivateKey {
// 	privateKeyBytes := ""
// 	privateKeyBlock, _ := pem.Decode([]byte(privateKeyBytes))
// 	if privateKeyBlock == nil || privateKeyBlock.Type != "PRIVATE KEY" {
// 		PrintOnlyErrMsg("Failed to decode public key")
// 		return nil
// 	}

// 	publicKeyInterface, err := x509.ParsePKIXPublicKey(privateKeyBlock.Bytes)
// 	if err != nil {
// 		PrintOnlyErrMsg("Failed to parse key")
// 		return nil
// 	}

// 	privateKey, ok := publicKeyInterface.(*rsa.PrivateKey)
// 	if !ok {
// 		PrintOnlyErrMsg("Failed to convert key")
// 		return nil
// 	}
// 	return privateKey
// }
