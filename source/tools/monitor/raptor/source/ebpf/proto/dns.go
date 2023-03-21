package proto

import (
	"errors"
	"strconv"
	"fmt"
	//"github.com/chentao-kernel/cloud_ebpf/util"
)
/*
All communications inside of the domain protocol are carried in a single
format called a message.  The top level format of message is divided
into 5 sections (some of which are empty in certain cases) shown below:

    +---------------------+
    |        Header       |
    +---------------------+
    |       Question      | the question for the name server
    +---------------------+
    |        Answer       | RRs answering the question
    +---------------------+
    |      Authority      | RRs pointing toward an authority
    +---------------------+
    |      Additional     | RRs holding additional information
    +---------------------+
*/

/*
 *
 * 0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                      ID                       |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                    QDCOUNT                    |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                    ANCOUNT                    |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                    NSCOUNT                    |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                    ARCOUNT                    |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *
 */
 const (
	DNS_HEADER_SIZE  = 12
 )

/*
* just care the recode and the domain name, recode parsed from kernel
*/
var recodeInfo = map[uint32]string {
	0:	"success",
	1:	"query format error",
	2:	"server failuer",
	3:	"domain name error",
	4:	"query not support",
	5:	"server refused",
}

func parseDnsRequest(msg *MsgData, pst *ParserResult) error {
	return nil
}

func parseDomainName(msg []byte, off int) (s string, off1 int, err error) {
	s = ""
	ptr := 0 // number of pointers followed
	loop_count := 0 // for dead loop
Loop:
	for {
		if off >= len(msg) {
			return "", len(msg), errors.New("offset big")
		}
		c := int(msg[off])
		off++
		switch c & 0xC0 {
		case 0x00:
			if c == 0x00 {
				// end of name
				break Loop
			}
			// literal string
			if off+c > len(msg) {
				return "", len(msg), errors.New("offset big")
			}
			s += string(msg[off:off+c]) + "."
			off += c
		case 0xC0:
			// pointer to somewhere else in msg.
			// remember location after first ptr,
			// since that's how many bytes we consumed.
			// also, don't follow too many pointers --
			// maybe there's a loop.
			if off >= len(msg) {
				return "", len(msg), errors.New("offset too big")
			}
			c1 := msg[off]
			off++
			if ptr == 0 {
				off1 = off
			}
			if ptr++; ptr > 10 {
				return "", len(msg), errors.New("parse failed")
			}
			off = (c^0xC0)<<8 | int(c1)
		default:
			// 0x80 and 0x40 are reserved
			return "", len(msg), errors.New("parser failed")
		}
		if loop_count > 1024 {
			return "", 1024, errors.New("dead loop")
		} else {
			loop_count++
		}
	}
	if ptr == 0 {
		off1 = off
	}
	return s, off1, nil
}

func setDnsResult(msg *MsgData, pst *ParserResult) {
	if (msg.Header.Recode > 5) {
		pst.ResStatus = "UnKnown"
	} else {
		pst.ResStatus = recodeInfo[msg.Header.Recode]
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
	pst.Recode = msg.Header.Recode
	pst.Pid = msg.Header.Pid
	pst.Comm = msg.Header.Comm
	pst.ProtoFamily = PROTO_IPV4
	pst.ResponseTimeMs, _ = strconv.ParseFloat(fmt.Sprintf("%.2f",
							float64(msg.Header.RspTimeNs) / float64(1000000)), 64)
}

func parseDnsReponse(msg *MsgData, pst *ParserResult) error {
	msg.Offset += DNS_HEADER_SIZE

	domain, offset, err := parseDomainName(msg.Data, msg.Offset)
	if err != nil {
		return err
	}

	pst.Url = domain
	msg.Offset += offset
	setDnsResult(msg, pst)

	return nil
}

func DnsParse(msg *MsgData, pst *ParserResult) error {
	if (msg == nil || pst == nil) {
		return errors.New("Parameter invalid")
	}
	if msg.Header.Type ==  MSG_RESPONSE {
		return parseDnsReponse(msg, pst)
	} else if msg.Header.Type == MSG_REQUEST {
		return parseDnsRequest(msg, pst)
	}
	return errors.New("Msg type unknow")
}