#[macro_export]
macro_rules! init_zeroed {
    () => {
        unsafe { std::mem::MaybeUninit::zeroed().assume_init() }
    }
}

#[macro_export]
macro_rules! to_vecu8 {
    ($item: expr, $type: ty) => {
        unsafe {
            std::slice::from_raw_parts(
                $item as *const $type as *const u8,
                std::mem::size_of::<$type>(),
            )
            .to_vec()
        }
    }
}


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

/// $first
macro_rules! struct_members_max_assign {
    ($assign: ident, $first: ident, $second: ident, $($mem: ident), +) => {

        $( $assign.$mem = $first.$mem.max($second.$mem); ) +
    };
}

/// $first
macro_rules! struct_members_min_assign {
    ($assign: ident, $first: ident, $second: ident, $($mem: ident), +) => {

        $( $assign.$mem = $first.$mem.min($second.$mem);) +
    };
}


/// 
macro_rules! struct_members_normalization_assign {
    ($assign: ident, $target: ident, $min: ident, $max: ident, $precision: ident, $($mem: ident), +) => {

        $( $assign.$mem = $target.$mem * $precision / 1.max($max.$mem - $min.$mem);) +
    };
}


pub(crate) use {
    same_struct_member_sub, struct_members_max_assign, struct_members_min_assign,
    struct_members_normalization_assign
};
