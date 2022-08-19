use anyhow::Result;

macro_rules! define_perf_event_channel {
    () => {
        static GLOBAL_TX: Lazy<Mutex<Option<crossbeam_channel::Sender<(usize, Vec<u8>)>>>> =
            Lazy::new(|| Mutex::new(None));

        pub fn handle_event(_cpu: i32, data: &[u8]) {
            let event = Vec::from(data);
            GLOBAL_TX
                .lock()
                .unwrap()
                .as_ref()
                .unwrap()
                .send((_cpu as usize, event))
                .unwrap();
        }

        pub fn handle_lost_events(cpu: i32, count: u64) {
            eprintln!("Lost {} events on CPU {}", count, cpu);
        }
    };
}

macro_rules! defined_skel_load {
    ($skelbuilder: ident) => {
        pub fn load(
            &mut self,
            debug: bool,
            btf: &Option<String>,
            enables: Option<Vec<&str>>,
            disables: Option<Vec<&str>>,
        ) -> Result<()> {
            let btf_cstring;
            let mut btf_cstring_ptr = std::ptr::null();
            if let Some(x) = btf {
                btf_cstring = std::ffi::CString::new(x.clone())?;
                btf_cstring_ptr = btf_cstring.as_ptr();
            }

            let mut skel_builder = $skelbuilder::default();
            skel_builder.obj_builder.debug(debug);
            let mut open_opts = skel_builder.obj_builder.opts(std::ptr::null());
            open_opts.btf_custom_path = btf_cstring_ptr;
            let mut open_skel = skel_builder.open_opts(open_opts)?;

            if let Some(xs) = enables {
                for x in xs {
                    if let Some(y) = open_skel.obj.prog_mut(x) {
                        y.set_autoload(true)?;
                    } else {
                        bail!("failed to find program: {}", x)
                    }
                }
            }

            if let Some(xs) = disables {
                for x in xs {
                    if let Some(y) = open_skel.obj.prog_mut(x) {
                        y.set_autoload(false)?;
                    } else {
                        bail!("failed to find program: {}", x)
                    }
                }
            }
            self.skel = open_skel.load()?;
            Ok(())
        }
    };
}

macro_rules! defined_skel_attach {
    () => {
        pub fn attach(&mut self, src: Option<&str>, dst: Option<&str>) -> Result<()> {
            if let Some(x) = src {
                if let Some(y) = dst {

                }
            }
            Ok(self.skel.attach()?)
        }
    }
}

pub(crate) use define_perf_event_channel;
pub(crate) use defined_skel_load;
pub(crate) use defined_skel_attach;

pub trait BpfSkel {
    fn load(
        &mut self,
        debug: bool,
        btf: &Option<&str>,
        enables: Option<Vec<&str>>,
        disables: Option<Vec<&str>>,
    ) -> Result<()>;
    fn attach(&mut self, src: Option<&str>, dst: Option<&str>) -> Result<()>;
}
