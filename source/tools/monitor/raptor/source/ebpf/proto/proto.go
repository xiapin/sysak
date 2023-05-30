package proto

type EventInfo struct {
}

type PROTO_TYPE int32

const (
	PROTO_UNKNOW PROTO_TYPE = 0
	PROTO_HTTP   PROTO_TYPE = 1
	PROTO_MYSQL  PROTO_TYPE = 2
	PROTO_DNS    PROTO_TYPE = 3
	PROTO_REDIS  PROTO_TYPE = 4
)

var PROTO = [...]string{
	"UnKnow",
	"http",
	"mysql",
	"dns",
	"redis",
}

type MSG_TYPE uint16

const (
	MSG_UNKNOW   MSG_TYPE = 0
	MSG_REQUEST  MSG_TYPE = 1
	MSG_RESPONSE MSG_TYPE = 2
)

type PARSE_STATUS uint16

const (
	PARSE_SUCESS PARSE_STATUS = 0
	PSRSE_FAIL   PARSE_STATUS = 1
)

type PROTO_FAMILY uint16

const (
	PROTO_IPV4 PROTO_FAMILY = 0
	PROTO_IPV6 PROTO_FAMILY = 1
)

type ParserResult struct {
	Proto          string
	Url            string
	Sip            string
	Dip            string
	Sport          uint16
	Dport          uint16
	Method         string
	Recode         uint32
	ResStatus      string
	ReqBytes       uint32
	ResBytes       uint32
	Pid            uint32
	Comm           string
	ContainerName  string
	ContainerId    uint32
	EvenType       uint32
	Status         PARSE_STATUS
	ProtoFamily    PROTO_FAMILY
	ResponseTimeMs float64
	Version        string
}

type ParseProtoCb func(msg *MsgData, parserRst *ParserResult) error

type MsgHeader struct {
	Pid       uint32
	Comm      string
	Fd        uint32
	Dip       string
	Sip       string
	Dport     uint16
	Sport     uint16
	RspTimeNs uint64
	Recode    uint32
	Type      MSG_TYPE
	Proto     PROTO_TYPE
}

type MsgData struct {
	Header MsgHeader
	Data   []byte
	Offset int
}

type MsgParser struct {
	Proto     string
	ParserRst ParserResult
	Msg       *MsgData
	ParseCb   ParseProtoCb
}

func (msg *MsgData) ParseBlank(from int) (int, []byte) {
	var length = len(msg.Data)

	for i := from; i < length; i++ {
		if msg.Data[i] == ' ' {
			return i + 1, msg.Data[from:i]
		}
	}
	return length, msg.Data[from:length]
}

func (msg *MsgData) ParseBlankSize(from int, size int) (int, []byte) {
	var length = len(msg.Data)
	if size+from < length {
		length = from + size
	}

	for i := from; i < length; i++ {
		if msg.Data[i] == ' ' {
			return i + 1, msg.Data[from:i]
		}
	}
	return length, msg.Data[from:length]
}

func (msg *MsgData) ParseCrlf(from int) (offset int, data []byte) {
	var length = len(msg.Data)
	if from >= length {
		return -1, nil
	}

	for i := from; i < length; i++ {
		if msg.Data[i] != '\r' {
			continue
		}

		if i == length-1 {
			// End with \r
			offset = length
			data = msg.Data[from : length-1]
			return
		} else if msg.Data[i+1] == '\n' {
			// \r\n
			offset = i + 2
			data = msg.Data[from:i]
			return
		} else {
			return -1, nil
		}
	}

	offset = length
	data = msg.Data[from:]
	return
}

func (parser *MsgParser) Parser(msg *MsgData) error {
	return parser.ParseCb(msg, &parser.ParserRst)
}

type ProtoParser interface {
	Parser(msg []byte, proto string) error
}

func NewProtoParser(proto string, msg *MsgData, parseCb ParseProtoCb) *MsgParser {
	return &MsgParser{
		Proto:   proto,
		Msg:     msg,
		ParseCb: parseCb,
	}
}
