/*
 * Consd vma scan
 */
#include <linux/kernel-page-flags.h>

#include "pagescan.h"
#include "kernel/mm_types.h"

/*
 * <Consd Vma>
 * type               size (kb)  ratio (%)
 * rss                1274288    -
 * anon_thp           159744     12.54
 * shmem_thp          0          0.00
 * file_thp           0          0.00
 * total_thp          159744     12.54
 */

void scan_consd_vma(struct procmap *procmap, int procmap_num)
{
	uint64_t rss = 0, anon_thp = 0, shmem_thp = 0, file_thp = 0, total_thp = 0;
	int procmap_idx;

	for (procmap_idx = 0; procmap_idx < procmap_num; procmap_idx++) {
		rss += procmap[procmap_idx].stat.counter[RSS];
		anon_thp += procmap[procmap_idx].stat.counter[ANONHUGEPAGES];
		shmem_thp += procmap[procmap_idx].stat.counter[SHMEMPMDMAPPED];
		file_thp += procmap[procmap_idx].stat.counter[FILEPMDMAPPED];
	}
	total_thp = anon_thp + shmem_thp + file_thp;

	LOG_INFO("<Consd Vma>\n");
	LOG_INFO("%-18s %-10s %-10s\n", "type", "size (kb)", "ratio (%)");
	LOG_INFO("%-18s %-10lu -\n", "rss", rss);
	LOG_INFO("%-18s %-10lu %-10.2f\n", "anon_thp", anon_thp, anon_thp * 100.0 / rss);
	LOG_INFO("%-18s %-10lu %-10.2f\n", "shmem_thp", shmem_thp, shmem_thp * 100.0 / rss);
	LOG_INFO("%-18s %-10lu %-10.2f\n", "file_thp", file_thp, file_thp * 100.0 / rss);
	LOG_INFO("%-18s %-10lu %-10.2f\n", "total_thp", total_thp, total_thp * 100.0 / rss);
	LOG_INFO("\n");
}
