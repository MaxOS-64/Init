/**
 * @file main.cpp
 * @brief Entry point for the MaxOS Init program
 *
 * @date 10th December 2025
 * @author Max Tyson
 */

#include <cstdint>

#include <libkpi/processes/thread.h>
#include <libkpi/ipc/sharedmemory.h>
#include <libcommon/mem.h>
#include <libcommon/string.h>
#include <libfs/server/fileserver_client.h>
#include <libfs/directory.h>
#include <libfs/file.h>
#include <libfs/path.h>

using namespace MaxOS::KPI::processes;
using namespace MaxOS::KPI::ipc;
using namespace MaxOS::KPI;
using namespace MaxOS;
using namespace MaxOS::common;
using namespace LibFS;

static void wait_for_disks_mounted() {

    const char* root = "/1/";
    uint64_t dir = 0;
    for (size_t tries = 0; tries < 20000 && !dir; tries++) {
        dir = open_directory(root);
        if (!dir)
            yield();
    }

    if (!dir) {
        klog("FileServer never mounted disks\n");
        while (true);
    }
}

static bool start_serivce(const entry_information_t* entry, const char* name) {

    // Not a service
    if (!entry->is_file)
        return true;

    string service_path = "/0/os/servers/"s + string(name);
    exec_file(Path::file_name(service_path).c_str(), service_path.c_str());

    return true;
}

extern "C" void _start(int argc, char* argv[]) {
    // Parse the ram disk
    klog("welcome to userspace\n");
    auto size = *(volatile uint32_t*)0xFFF000;
    auto header = (struct tar_header*)(0xFFF000 + sizeof(uint32_t));

    // Pass ramdisk to fileserver
    auto address = create_shared_memory("init_initrd", size);
    if (!address) {
        klog("Init: initrd region FAILED\n");
        exit(1);
    }

    memcpy(address, header, size);
    wait_for_fileserver_server();
    mount_ramdisk("init_initrd", size);
    klog("ramdisk mounted\n");

    // Start driver manager
    string dm_path = "/0/boot/initrd/DriverManager.elf";
    exec_file(Path::file_name(dm_path).c_str(), dm_path.c_str());
    klog("DriverManager launched\n");

    // Wait for fileserver to be ready
    wait_for_disks_mounted();

    // Get services
    uint64_t services_dir = open_directory("/0/os/servers/");
    size_t services_size = directory_entries_size(services_dir);
    auto entries = new uint8_t[services_size];

    // Start core services
    directory_entries(services_dir, entries, services_size);
    iterate_entries(entries, services_size, start_serivce);

    // Handoff to user login

    // Don't be scheduled again, but cant kms as that would kill child processes and thus core servers
    while (true)
        sleep(UINT32_MAX);
}
