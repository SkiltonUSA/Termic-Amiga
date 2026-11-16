"""Portable ordering checks against the actual monitor comparator.

Native lifecycle/UI checks live in amiga/native/tests/test_activity.c.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class ActivityMonitorTests(unittest.TestCase):
    def test_sort_direction_ties_and_full_width_counters(self):
        main = (ROOT/'amiga/apps/process-monitor/main.c').read_text()
        monitor = (ROOT/'amiga/apps/process-monitor/monitor.h').read_text()
        functions = monitor.split('static int address_order', 1)[1].split('static void snapshot', 1)[0]
        names = main.split('static const char *displayname', 1)[1].split('static void refresh', 1)[0]
        source = r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef uintptr_t ULONG;
typedef struct {void *task; uint32_t hits,tracked; char name[80],command[256];} Row;
static int sort_order;
''' + 'static int address_order' + functions + 'static const char *displayname' + names + r'''
int main(void) {
 Row rows[3]={{(void *)0x3000,1,UINT32_MAX,"c","Example:Programs/Z"},
              {(void *)0x1000,9,0,"a","Example:Programs/A"},
              {(void *)0x2000,9,65536,"b",""}};
 sort_order=1;qsort(rows,3,sizeof(Row),activity_order);
 assert(rows[0].task==(void *)0x1000 && rows[1].task==(void *)0x2000 && rows[2].hits==1);
 sort_order=2;qsort(rows,3,sizeof(Row),activity_order);
 assert(rows[0].tracked==UINT32_MAX && rows[2].tracked==0);
 sort_order=3;qsort(rows,3,sizeof(Row),activity_order);
 assert(!strcmp(displayname(&rows[0]),"A") && !strcmp(displayname(&rows[2]),"b"));
 sort_order=0;qsort(rows,3,sizeof(Row),activity_order);
 assert(rows[0].task==(void *)0x1000 && rows[2].task==(void *)0x3000);
 assert(activity_order(&rows[0],&rows[0])==0);
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            c=Path(d)/'sort.c';binary=Path(d)/'sort';c.write_text(source)
            subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',
                            '-fsanitize=address,undefined',str(c),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)

if __name__ == '__main__':
    unittest.main()
