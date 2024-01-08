package common

import (
    // "crypto/rand"
    // "crypto/rsa"
    // "crypto/x509"
    // "encoding/pem"
    "encoding/base64"
    // "flag"
)

func GetUsersInfo() (string, string) {
    username, password, secretType := GetRawUsersInfo()
    if secretType != "" {
        username = decryptData(username, secretType)
        password = decryptData(password, secretType)
    }
    return username, password
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
    //     PrintOnlyErrMsg("Failed to decrypt data: %s", data)
    //     return ""
    // }
    // return string(decryptedData)
    return string(data)
}

// func decodeSecretKey() *rsa.PrivateKey {
//     privateKeyBytes := ""
//     privateKeyBlock, _ := pem.Decode([]byte(privateKeyBytes))
//     if privateKeyBlock == nil || privateKeyBlock.Type != "PRIVATE KEY" {
//         PrintOnlyErrMsg("Failed to decode public key")
//         return nil
//     }

//     publicKeyInterface, err := x509.ParsePKIXPublicKey(privateKeyBlock.Bytes)
//     if err != nil {
//         PrintOnlyErrMsg("Failed to parse key")
//         return nil
//     }

//     privateKey, ok := publicKeyInterface.(*rsa.PrivateKey)
//     if !ok {
//         PrintOnlyErrMsg("Failed to convert key")
//         return nil
//     }
//     return privateKey
// }
