use cenum_rs::CEnum;

#[test]
fn test_basic() {
    #[derive(Debug, PartialEq, CEnum)]
    #[cenum(i32)]
    enum ProtocolType {
        #[cenum(value = "libc::IPPROTO_ICMP", display = "Icmp")]
        Icmp,
        #[cenum(value = "libc::IPPROTO_TCP", display = "Tcp")]
        Tcp,
        #[cenum(value = "libc::IPPROTO_UDP", display = "Udp")]
        Udp,
    }

    assert_eq!(
        ProtocolType::Icmp,
        ProtocolType::try_from(libc::IPPROTO_ICMP).unwrap()
    );
    assert_eq!(
        ProtocolType::Tcp,
        ProtocolType::try_from(libc::IPPROTO_TCP).unwrap()
    );
    assert_eq!(
        ProtocolType::Udp,
        ProtocolType::try_from(libc::IPPROTO_UDP).unwrap()
    );
}
