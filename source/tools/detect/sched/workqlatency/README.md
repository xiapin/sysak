# 功能说明
工作队列(workqueue)延时跟踪工具

使用说明:
Usage: sysak workqlatency [options]
Options:
    --time/-t		specify the monitor period(s), default=10s
    --help/-h		help info
example: sysak workqlatency -t 100  #trace work runtime and latency statistics


使用示例：sysak worklatency -t 10 监控10‘s内内存直接回收规整情况
Kwork Name                                              Cpu             Avg delay(ns)   Count           Max delay(ns)   Max delay start(s)      Max delay end(s)
<0xffffffff9245ade0> blk_mq_timeout_work                1               6507            1               6507            2022-08-14 09:57:13     2022-08-14 09:57:13
<0xffffffff92225a10> vmstat_shepherd                    0               1994208015      5               3964009175      2022-08-14 09:57:11     2022-08-14 09:57:15
<0xffffffff927893d0> neigh_periodic_work                3               7455            1               7455            2022-08-14 09:57:13     2022-08-14 09:57:13
<0xffffffff9245ce70> blk_mq_requeue_work                3               7209            1               7209            2022-08-14 09:57:13     2022-08-14 09:57:13
<0xffffffff92817f30> check_lifetime                     1               1536003307      4               3072001836      2022-08-14 09:57:11     2022-08-14 09:57:14
<0xffffffff92225cc0> vmstat_update                      0               1501752777      4               3004002826      2022-08-14 09:57:11     2022-08-14 09:57:14
<0xffffffff927893d0> neigh_periodic_work                3               4397            1               4397            2022-08-14 09:57:13     2022-08-14 09:57:13
<0xffffffff921bf650> bpf_prog_free_deferred             0               3574            1               3574            2022-08-14 09:57:11     2022-08-14 09:57:11
<0xffffffff9212caa0> sync_hw_clock                      1               1971205503      5               3968005388      2022-08-14 09:57:11     2022-08-14 09:57:15
<0xffffffff922e3c40> wb_workfn                          1               5326            1               5326            2022-08-14 09:57:13     2022-08-14 09:57:13
<0xffffffff920d0460> calc_stress_avgs_work              2               6307            1               6307            2022-08-14 09:57:15     2022-08-14 09:57:15
<0xffffffff9245ce70> blk_mq_requeue_work                2               168676          2               333326          2022-08-14 09:57:13     2022-08-14 09:57:13
<0xffffffffc02d3f10> do_cache_clean                     3               5527            1               5527            2022-08-14 09:57:15     2022-08-14 09:57:15
<0xffffffff92225cc0> vmstat_update                      3               5371            1               5371            2022-08-14 09:57:12     2022-08-14 09:57:12
<0xffffffff92225cc0> vmstat_update                      1               4863            1               4863            2022-08-14 09:57:12     2022-08-14 09:57:12
<0xffffffff9250f410> fb_flashcursor                     2               2391214034      24              4784002476      2022-08-14 09:57:11     2022-08-14 09:57:15
<0xffffffff921bf650> bpf_prog_free_deferred             0               5724            1               5724            2022-08-14 09:57:11     2022-08-14 09:57:11
<0xffffffff92225cc0> vmstat_update                      2               1003672785      3               1985000231      2022-08-14 09:57:11     2022-08-14 09:57:13
Kwork Name                                              Cpu             Avg runtime(ns) Count           Max runtime(ns) Max runtime start(s)    Max runtime end(s)
<0xffffffff92225cc0> vmstat_update                      1               477998675       2               955994612       2022-08-14 09:57:11     2022-08-14 09:57:12
<0xffffffff922e3c40> wb_workfn                          1               5992            1               5992            2022-08-14 09:57:13     2022-08-14 09:57:13
<0xffffffff920b9880> wq_barrier_func                    1               1743            1               1743            2022-08-14 09:57:11     2022-08-14 09:57:11
<0xffffffff9212caa0> sync_hw_clock                      1               1971203445      5               3968003172      2022-08-14 09:57:11     2022-08-14 09:57:15
<0xffffffff9259c900> flush_to_ldisc                     2               127273          22              249177          2022-08-14 09:57:15     2022-08-14 09:57:15
<0xffffffff9245ce70> blk_mq_requeue_work                2               166215          2               330489          2022-08-14 09:57:13     2022-08-14 09:57:13
<0xffffffff921bf650> bpf_prog_free_deferred             0               21212           1               21212           2022-08-14 09:57:11     2022-08-14 09:57:11
<0xffffffff920b9880> wq_barrier_func                    1               93038           2               184218          2022-08-14 09:57:15     2022-08-14 09:57:15
<0xffffffff920d0460> calc_stress_avgs_work              2               3087            1               3087            2022-08-14 09:57:15     2022-08-14 09:57:15
<0xffffffff92211f80> lru_add_drain_per_cpu              2               6012            1               6012            2022-08-14 09:57:11     2022-08-14 09:57:11
<0xffffffff927893d0> neigh_periodic_work                3               799             1               799             2022-08-14 09:57:13     2022-08-14 09:57:13
<0xffffffff920b9880> wq_barrier_func                    2               159978          11              325888          2022-08-14 09:57:15     2022-08-14 09:57:15
<0xffffffffc02d3f10> do_cache_clean                     3               1619            1               1619            2022-08-14 09:57:15     2022-08-14 09:57:15
<0xffffffff92225cc0> vmstat_update                      2               1003669541      3               1984996259      2022-08-14 09:57:11     2022-08-14 09:57:13
<0xffffffff92225cc0> vmstat_update                      0               1501748257      4               3003998398      2022-08-14 09:57:11     2022-08-14 09:57:14
<0xffffffff927893d0> neigh_periodic_work                3               3725            1               3725            2022-08-14 09:57:13     2022-08-14 09:57:13
<0xffffffff9245ce70> blk_mq_requeue_work                3               2701            1               2701            2022-08-14 09:57:13     2022-08-14 09:57:13
<0xffffffff92817f30> check_lifetime                     1               1536001123      4               3071999724      2022-08-14 09:57:11     2022-08-14 09:57:14
<0xffffffff92225cc0> vmstat_update                      3               481996651       2               963990662       2022-08-14 09:57:11     2022-08-14 09:57:12
<0xffffffff9245ade0> blk_mq_timeout_work                1               1860            1               1860            2022-08-14 09:57:13     2022-08-14 09:57:13
<0xffffffff9259c900> flush_to_ldisc                     1               4722740153      18              5000711191      2022-08-14 09:57:10     2022-08-14 09:57:15
<0xffffffff92211f80> lru_add_drain_per_cpu              1               5557            1               5557            2022-08-14 09:57:11     2022-08-14 09:57:11
<0xffffffff9250f410> fb_flashcursor                     2               2391213317      24              4784001660      2022-08-14 09:57:11     2022-08-14 09:57:15
<0xffffffff921bf650> bpf_prog_free_deferred             0               29087           1               29087           2022-08-14 09:57:11     2022-08-14 09:57:11
