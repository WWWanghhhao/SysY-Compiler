#include "physical_register.h"
#include "liveinterval.h"
bool PhysicalRegistersAllocTools::OccupyReg(int phy_id, LiveInterval interval) {
    // 你需要保证interval不与phy_id已有的冲突
    // 或者增加判断分配失败返回false的代码

    auto occupies = phy_occupied[phy_id];
    if (occupies.empty()) {
        // 如果当前物理寄存器的使用区间为空，则可以分配
        phy_occupied[phy_id].push_back(interval);
    } else {
        // 遍历所有占据的区间，只要有冲突就返回 false
        for (auto ival : occupies) {
            if (ival & interval) {
                return false;
            }
        }
        phy_occupied[phy_id].push_back(interval);
    }
    return true;
}

bool PhysicalRegistersAllocTools::ReleaseReg(int phy_id, LiveInterval interval) { TODO("ReleaseReg"); }

bool PhysicalRegistersAllocTools::OccupyMem(int offset, LiveInterval interval) {
    // TODO("OccupyMem");

    // offset /= 4;
    while (offset >= mem_occupied.size()) {
        mem_occupied.push_back({});
    }
    auto occupies = mem_occupied[offset];
    if (occupies.empty()) {
        mem_occupied[offset].push_back(interval);
    } else {
        for (auto ival : occupies) {
            if (ival & interval) {
                return false;
            }
        }
        mem_occupied[offset].push_back(interval);
    }
    return true;
}
bool PhysicalRegistersAllocTools::ReleaseMem(int offset, LiveInterval interval) {
    TODO("ReleaseMem");
    return true;
}

int PhysicalRegistersAllocTools::getIdleReg(LiveInterval interval) {
    // TODO("getIdleReg");

    // 获取可用的物理寄存器
    auto candidates = getValidRegs(interval);

    for (auto i : candidates) {
        // 尝试占据物理寄存器，占据成功表示可以使用
        if (OccupyReg(i, interval)) {
            return i;
        }
    }
    return -1;
}
int PhysicalRegistersAllocTools::getIdleMem(LiveInterval interval) {
    // TODO("getIdleMem");

    if (mem_occupied.empty()) {
        for (int i = 0; i < 50; i++) {
            mem_occupied.push_back({});
        }
    }
    for (int i = 0; i < mem_occupied.size(); i++) {
        if (OccupyMem(i, interval)) {
            // std::cout << i << " -\n";
            return i;
        }
    }
    return -1;
}

int PhysicalRegistersAllocTools::swapRegspill(int p_reg1, LiveInterval interval1, int offset_spill2, int size,
                                              LiveInterval interval2) {

    TODO("swapRegspill");
    return 0;
}

std::vector<LiveInterval> PhysicalRegistersAllocTools::getConflictIntervals(LiveInterval interval) {
    std::vector<LiveInterval> result;
    for (auto phy_intervals : phy_occupied) {
        for (auto other_interval : phy_intervals) {
            if (interval.getReg().type == other_interval.getReg().type && (interval & other_interval)) {
                result.push_back(other_interval);
            }
        }
    }
    return result;
}
