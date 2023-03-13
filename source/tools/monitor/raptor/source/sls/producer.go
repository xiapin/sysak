package sls

import (
	"fmt"
	"time"

	"github.com/aliyun/aliyun-log-go-sdk/producer"
)

var SlsProducer *SLSProducer

type Callback struct {
}

type SLSProducer struct {
	Config   *producer.ProducerConfig
	Instance *producer.Producer
	Project  string
	Logstore string
	CallBack *Callback
}

func (callback *Callback) Success(result *producer.Result) {
	attemptList := result.GetReservedAttempts()
	for _, attempt := range attemptList {
		fmt.Println(attempt)
	}
}

func (callback *Callback) Fail(result *producer.Result) {
	fmt.Println(result.IsSuccessful())
	fmt.Println(result.GetErrorCode())
	fmt.Println(result.GetErrorMessage())
	fmt.Println(result.GetReservedAttempts())
	fmt.Println(result.GetRequestId())
	fmt.Println(result.GetTimeStampMs())
}

func NewSLSProducer(endpoint string, akid string, akse string,
	project string, logstore string) *SLSProducer {
	config := producer.GetDefaultProducerConfig()
	config.Endpoint = endpoint
	config.AccessKeyID = akid
	config.AccessKeySecret = akse

	return &SLSProducer{
		Config:   config,
		Project:  project,
		Logstore: logstore,
		CallBack: &Callback{},
	}
}

func (p *SLSProducer) Init() {
	p.Instance = producer.InitProducer(p.Config)
	p.Instance.Start()
}

func (p *SLSProducer) Send(text map[string]string) error {
	//fmt.Printf("endpoint:%s, id:%s, secret:%s, text:%v\n", p.Config.Endpoint, p.Config.AccessKeyID, p.Config.AccessKeySecret, text)
	log := producer.GenerateLog(uint32(time.Now().Unix()), text)
	err := p.Instance.SendLog(p.Project, p.Logstore, "topic", "127.0.0.1", log)
	return err
}

func (p *SLSProducer) SendWithCallBack(text map[string]string) error {
	//fmt.Printf("project:%s, logstore:%s, endpoint:%s, id:%s, secret:%s, text:%v\n", p.Project, p.Logstore, p.Config.Endpoint, p.Config.AccessKeyID, p.Config.AccessKeySecret, text)
	log := producer.GenerateLog(uint32(time.Now().Unix()), text)
	err := p.Instance.SendLogWithCallBack(p.Project, p.Logstore, "topic", "127.0.0.1", log, p.CallBack)
	return err
}

func (p *SLSProducer) Close(timeoutMs int64) error {
	return p.Instance.Close(timeoutMs)
}
