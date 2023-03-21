package sls

import (
	"bytes"
	"fmt"
	"io"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	Sls "github.com/aliyun/aliyun-log-go-sdk"
	consumerLibrary "github.com/aliyun/aliyun-log-go-sdk/consumer"
	"github.com/go-kit/kit/log/level"
)

var SlsConsumer *SLSConsumer

type SLSConsumer struct {
	Config consumerLibrary.LogHubConfig
	SigCtl chan os.Signal
	client *http.Client
}

func (c *SLSConsumer) Init() {
	worker := consumerLibrary.InitConsumerWorker(c.Config, c.process)
	signal.Notify(c.SigCtl, syscall.SIGHUP, syscall.SIGINT, syscall.SIGTERM, syscall.SIGQUIT, syscall.SIGUSR1, syscall.SIGUSR2)
	worker.Start()
	if _, ok := <-c.SigCtl; ok {
		level.Info(worker.Logger).Log("msg", "get stop signal, start to stop consumer worker", "consumer worker name", c.Config.ConsumerName)
		worker.StopAndWait()
	}
}

func (c *SLSConsumer) UploadData(url string, data string) error {
	request, err := http.NewRequest("POST", url, bytes.NewReader([]byte(data)))
	if err != nil {
		return fmt.Errorf("new http request: %v", err)
	}
	request.Header.Set("Content-Type", "binary/octet-stream+trie")

	// do the request and get the response
	response, err := c.client.Do(request)
	if err != nil {
		return fmt.Errorf("do http request: %v", err)
	}
	defer response.Body.Close()

	// read all the response body
	respBody, err := io.ReadAll(response.Body)
	if err != nil {
		return fmt.Errorf("read response body: %v", err)
	}

	if response.StatusCode != 200 {
		return fmt.Errorf("failed to upload. server responded with statusCode: '%d' and body: '%s'", response.StatusCode, string(respBody))
	}
	return nil
}

func (c *SLSConsumer) SLSDeliverData(logGroupList *Sls.LogGroupList) error {
	for _, logGroup := range logGroupList.LogGroups {
		for _, log := range logGroup.Logs {
			for _, content := range log.Contents {
				url := content.Key
				data := content.Value
				//fmt.Printf("============deliver data===========\n")
				//fmt.Printf("url:%s, stack:%s\n", *url, *data)
				if err := c.UploadData(*url, *data); err != nil {
					fmt.Printf("Consumer upload data failed:%v\n", err)
				}
			}
		}
	}
	return nil
}

func NewSLSConsumer(endpoint string, akid string, akse string,
	project string, logstore string) *SLSConsumer {
	return &SLSConsumer{
		Config: consumerLibrary.LogHubConfig{
			Endpoint:          endpoint,
			AccessKeyID:       akid,
			AccessKeySecret:   akse,
			Project:           project,
			Logstore:          logstore,
			ConsumerGroupName: "raptorGroup",
			ConsumerName:      "raptor",
			// This options is used for initialization, will be ignored once consumer group is created and each shard has been started to be consumed.
			// Could be "begin", "end", "specific time format in time stamp", it's log receiving time.
			CursorPosition: consumerLibrary.END_CURSOR,
		},
		SigCtl: make(chan os.Signal),
		client: &http.Client{
			Transport: &http.Transport{
				MaxConnsPerHost: 4,
			},
			Timeout: 10 * time.Second,
		},
	}
}

// Fill in your consumption logic here, and be careful not to change the parameters of the function and the return value,
// otherwise you will report errors.
func (c *SLSConsumer) process(shardId int, logGroupList *Sls.LogGroupList) string {
	err := c.SLSDeliverData(logGroupList)
	if err != nil {
		fmt.Printf("sls deliver failed:%v\n", err)
	}
	//fmt.Println(shardId, logGroupList)
	return ""
}
