function sysalert(str) {
    console.log(`System module: ${str}`);
}

function memalert() {
    let mem_stats = System.getMemoryStats();

    sysalert("Memory counters:");

    sysalert("-----------------------------------------------------");

    sysalert(`Core: ${mem_stats.core} bytes`);
    sysalert(`Native stack: ${mem_stats.nativeStack} bytes`);
    sysalert(`Allocations: ${mem_stats.allocs} bytes`);

    sysalert("-----------------------------------------------------\n");

    return mem_stats;
}

sysalert("Starting Tests...\n");

let old_stat = memalert();
const old_alloc = old_stat.allocs;
old_stat = null;

sysalert("Allocation test, creating some arrays and objects...");

let test = Array.from({length: 512}, () => Math.floor(Math.random() * 512));
sysalert(JSON.stringify(test));
test = null;

std.gc();

let new_stat = memalert();

sysalert(`512 random numbers array: ${new_stat.allocs - old_alloc} bytes`);



System.sleep(0xFFFFFFFF);