package proto

import (
	"errors"
	"strings"
	"strconv"
	"fmt"
	//"github.com/chentao-kernel/cloud_ebpf/util"
)
//https://www.cnblogs.com/arthinking/p/14439304.html
/*
// request line
GET /admin_ui/rdx/core/images/close.png HTTP/1.1
// request header
Accept:
Referer: http://xxx.xxx.xxx.xxx/menu/neo
Accept-Language: en-US
User-Agent: Mozilla/4.0
Accept-Encoding: gzip, deflate
Host: xxx.xxx.xxx.xxx
Connection: Keep-Alive
Cookie: startupapp=neo; is_cisco_platform=0;
// message
*/

/*
// status line
HTTP/1.1 200 OK
// reponse header
Bdpagetype: 1
Bdqid: 0xacbbb9d800005133
Cache-Control: private
Connection: Keep-Alive
Content-Encoding: gzip
Content-Type: text/html
Date: Fri, 12 Oct 2018 06:36:28 GMT
Expires: Fri, 12 Oct 2018 06:36:26 GMT
Server: BWS/1.1
Set-Cookie: delPer=0; path=/; domain=.baidu.com
// message
*/

var httpMethodInfo = map[string]bool{
	"GET":     true,
	"POST":    true,
	"DELETE":  true,
	"OPTIONS": true,
	"HEAD":    true,
	"PUT":     true,
	"TRACE":   true,
	"CONNECT": true,
}

var httpVersionInfo = []string {
	"http/1.0",
	"http/1.1",
}

var httpCodeInfo = map[int]string {
	1:"1xx",
	2:"2xx",
	3:"3xx",
	4:"4xx",
	5:"5xx",
}

func parseHeader(msg *MsgData) map[string]string {
	header := make(map[string]string)

	from, data := msg.ParseCrlf(0)
	if data == nil {
		return header
	}
	for {
		from, data = msg.ParseCrlf(from)
		if data == nil {
			return header
		}
		if position := strings.Index(string(data), ":"); position > 0 && position < len(data)-2 {
			header[strings.ToLower(string(data[0:position]))] = string(data[position+2:])
			continue
		}
		return header
	}
}

func setHttpResult(msg *MsgData, pst *ParserResult, header map[string]string) error {
	return nil
}

func parseHttpReponse(msg *MsgData, pst *ParserResult) error {
	_, status := msg.ParseBlankSize(msg.Offset, 6)
	recode, err := strconv.ParseInt(string(status), 10, 0)
	if err != nil {
		return err
	}
	tmp := recode / 100
	if v, ok := httpCodeInfo[int(tmp)]; ok {
		pst.Recode = uint32(recode)
		pst.ResStatus = v
	}
	_, version := msg.ParseBlankSize(msg.Offset, 9)
	if httpVersionInfo[0] == string(version) || httpVersionInfo[1] == string(version) {
		pst.Version = string(version)
	} else {
		pst.Version = "Unknow"
	}

	pst.Proto = PROTO[PROTO_DNS]
	pst.Dip = msg.Header.Dip
	pst.Dport = msg.Header.Dport
	/*
	pst.Sip = util.IntToIpv4(msg.Header.Sip)
	pst.Dip = util.IntToIpv4(msg.Header.Dip)
	pst.Dport = util.NetToHostShort(msg.Header.Dport)
	pst.Sport = util.NetToHostShort(msg.Header.Sport)
	*/
	pst.Pid = msg.Header.Pid
	pst.Comm = msg.Header.Comm
	pst.ProtoFamily = PROTO_IPV4
	pst.ResponseTimeMs, _ = strconv.ParseFloat(fmt.Sprintf("%.2f",
							float64(msg.Header.RspTimeNs) / float64(1000000)), 64)
	return nil
}

func parseHttpRequest(msg *MsgData, pst *ParserResult) error {

	offset, method := msg.ParseBlankSize(msg.Offset, 8)
	if httpMethodInfo[string(method)] {
		pst.Method = string(method)
	} else {
		pst.Method = "Unknow"
	}
	_, url := msg.ParseBlank(offset)
	pst.Url = string(url)

	pst.Proto = PROTO[PROTO_DNS]
	pst.Dip = msg.Header.Dip
	pst.Dport = msg.Header.Dport
	/*
	pst.Sip = util.IntToIpv4(msg.Header.Sip)
	pst.Dip = util.IntToIpv4(msg.Header.Dip)
	pst.Dport = util.NetToHostShort(msg.Header.Dport)
	pst.Sport = util.NetToHostShort(msg.Header.Sport)
	*/
	pst.Pid = msg.Header.Pid
	pst.ProtoFamily = PROTO_IPV4
	pst.ResponseTimeMs, _ = strconv.ParseFloat(fmt.Sprintf("%.2f",
							float64(msg.Header.RspTimeNs) / float64(1000000)), 64)

	return nil
}

func HttpParse(msg *MsgData, pst *ParserResult) error {
	if (msg == nil || pst == nil) {
		return errors.New("Parameter invalid")
	}
	if msg.Header.Type ==  MSG_RESPONSE {
		return parseHttpReponse(msg, pst)
	} else if msg.Header.Type == MSG_REQUEST {
		return parseHttpRequest(msg, pst)
	}
	return errors.New("Msg type unknow")
}