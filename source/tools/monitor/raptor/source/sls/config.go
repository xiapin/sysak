package sls

import (
	"bytes"
	"crypto/aes"
	"crypto/cipher"
	"encoding/base64"
	"fmt"
)

const (
	SLSPRODUCER   string = "producer"
	SLSPRODUCERAW string = "produceraw"
	SLSCONSUMER   string = "consumer"
	SLSUNUSER     string = "unuser"
	KEY           string = "1234567812345678"
)

func SLSEncrypt(text string) string {
	return AesEncrypt(text, KEY)
}

func SLSDecrypt(text string) string {
	return AesDecrypt(text, KEY)
}

func SLSInit(slsType string, endpoint string, akid string, akse string,
	project string, logstore string) error {
	var ak string
	var sk string
	if akid != "akid" && akse != "akse" {
		fmt.Printf("You should use encrypted akid/akse, the command is: rapotr oncpu --encrypt {raw akid/akse}\n")
		ak = SLSDecrypt(akid)
		sk = SLSDecrypt(akse)
	}
	if slsType == SLSCONSUMER {
		fmt.Printf("===========SLS CONSUMER START=========\n")
		c := NewSLSConsumer(endpoint, ak, sk, project, logstore)
		c.Init()
	} else if slsType == SLSPRODUCER || slsType == SLSPRODUCERAW {
		fmt.Printf("===========SLS PRODUCER START, TYPE:%s=========\n", slsType)
		SlsProducer = NewSLSProducer(endpoint, ak, sk, project, logstore)
		SlsProducer.Init()
	} else if slsType == SLSUNUSER {
	} else {
		return fmt.Errorf("sls type not defined:%s", slsType)
	}
	return nil
}

func AesEncrypt(orig string, key string) string {
	origData := []byte(orig)
	k := []byte(key)

	block, _ := aes.NewCipher(k)
	blockSize := block.BlockSize()
	origData = PKCS7Padding(origData, blockSize)
	blockMode := cipher.NewCBCEncrypter(block, k[:blockSize])
	cryted := make([]byte, len(origData))
	blockMode.CryptBlocks(cryted, origData)

	return base64.StdEncoding.EncodeToString(cryted)

}

func AesDecrypt(cryted string, key string) string {
	crytedByte, _ := base64.StdEncoding.DecodeString(cryted)
	k := []byte(key)

	block, _ := aes.NewCipher(k)
	blockSize := block.BlockSize()
	blockMode := cipher.NewCBCDecrypter(block, k[:blockSize])
	orig := make([]byte, len(crytedByte))
	blockMode.CryptBlocks(orig, crytedByte)
	orig = PKCS7UnPadding(orig)
	return string(orig)
}

func PKCS7Padding(ciphertext []byte, blocksize int) []byte {
	padding := blocksize - len(ciphertext)%blocksize
	padtext := bytes.Repeat([]byte{byte(padding)}, padding)
	return append(ciphertext, padtext...)
}

func PKCS7UnPadding(origData []byte) []byte {
	length := len(origData)
	unpadding := int(origData[length-1])
	return origData[:(length - unpadding)]
}
