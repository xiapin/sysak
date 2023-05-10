package sls

import (
	"fmt"
)

const (
	SLSPRODUCER   string = "producer"
	SLSPRODUCERAW string = "produceraw"
	SLSCONSUMER   string = "consumer"
	SLSUNUSER     string = "unuser"
)

func SLSInit(slsType string, endpoint string, akid string, akse string,
	project string, logstore string) error {

	if slsType == SLSCONSUMER {
		fmt.Printf("===========SLS CONSUMER START=========\n")
		c := NewSLSConsumer(endpoint, akid, akse, project, logstore)
		c.Init()
	} else if slsType == SLSPRODUCER || slsType == SLSPRODUCERAW {
		fmt.Printf("===========SLS PRODUCER START, TYPE:%s=========\n", slsType)
		SlsProducer = NewSLSProducer(endpoint, akid, akse, project, logstore)
		SlsProducer.Init()
	} else if slsType == SLSUNUSER {
	} else {
		return fmt.Errorf("sls type not defined:%s", slsType)
	}
	return nil
}
