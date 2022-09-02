
use std::fmt;
use std::io::Cursor;

pub struct Stack {
    stack: Vec<String>,
}

impl Stack {

    // fn stack_string(kallsyms: &Kallsyms, stack: Vec<u8>) -> Result<Vec<String>> {
    //     let stack_depth = stack.len() / 8;
    //     let mut rdr = Cursor::new(stack);
    //     let mut stackstring = Vec::new();
        
    //     for i in 0..stack_depth {
    //         let addr = rdr.read_u64::<NativeEndian>()?;
    //         if addr == 0 {
    //             break;
    //         }
    //         stackstring.push(kallsyms.addr_to_sym(addr));
    //     }
    //     Ok(stackstring)
    // }

    pub fn new(stack: Vec<u8>) -> Stack {


        Stack {
            stack: Vec::new()
        }
    }

}


impl fmt::Display for Stack {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        for stack in &self.stack {
            writeln!(f, "\t{}", stack)?;
        }
        write!(f, "")
    }
}