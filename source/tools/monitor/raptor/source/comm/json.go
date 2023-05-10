package comm

import (
	"bytes"
	"encoding/json"
	"fmt"
	"os"
)

type TableData struct {
	COMM             string
	TaskHitRatio     string
	HotStack         string
	HotStackHitRatio string
	COMMENT          string
}

type Table struct {
	Data []TableData `json:"data"`
}

type Datasource struct {
	AppProfile Table `json:"appProfile"`
}

type JsonFormat struct {
	Datasources Datasource `json:"datasources"`
}

func NewJsonFormat() *JsonFormat {
	return &JsonFormat{
		Datasources: Datasource{
			AppProfile: Table{
				Data: make([]TableData, 0),
			},
		},
	}
}

func (jf *JsonFormat) Append(v TableData) {
	jf.Datasources.AppProfile.Data = append(jf.Datasources.AppProfile.Data, v)
}

func (jf *JsonFormat) Marshal() ([]byte, error) {
	return json.Marshal(jf)
}

func (jf *JsonFormat) Print(v []byte) error {
	var out bytes.Buffer
	if err := json.Indent(&out, v, "", "\t"); err != nil {
		return fmt.Errorf("Json indent error:%v", err)
	}
	out.WriteTo(os.Stdout)
	return nil
}
