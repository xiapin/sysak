


macro_rules! struct_member_sub_assign {
    ($res: ident, $first: ident, $second: ident, $($mem: ident), +) => {

        $( $res.$mem = $first.$mem - $second.$mem; )+
    };
}


/// first  minuend
/// second  subtrahend
macro_rules! same_struct_member_sub {
    ($first: ident, $second: ident, $mem: ident) => {

        $first.$mem - $second.$mem
    };
}

pub(crate) use same_struct_member_sub;