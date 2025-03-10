#include "fast_linear_scan.h"
bool IntervalsPrioCmp(LiveInterval a, LiveInterval b) { return a.begin()->begin > b.begin()->begin; }
FastLinearScan::FastLinearScan(MachineUnit *unit, PhysicalRegistersAllocTools *phy, SpillCodeGen *spiller)
    : RegisterAllocation(unit, phy, spiller), unalloc_queue(IntervalsPrioCmp) {}
bool FastLinearScan::DoAllocInCurrentFunc() {
    bool spilled = false;
    auto mfun = current_func;
    PRINT("FastLinearScan: %s", mfun->getFunctionName().c_str());
    // std::cerr<<"FastLinearScan: "<<mfun->getFunctionName()<<"\n";
    phy_regs_tools->clear();
    for (auto interval : intervals) {
        Assert(interval.first == interval.second.getReg());
        if (interval.first.is_virtual) {
            // 需要分配的虚拟寄存器
            unalloc_queue.push(interval.second);
        } else {
            // Log("Pre Occupy Physical Reg %d",interval.first.reg_no);
            // 预先占用已经存在的物理寄存器
            auto it = phy_regs_tools->OccupyReg(interval.first.reg_no, interval.second);
            // std::cout << "Result: " << it << ", RegId: " << interval.first.reg_no << "\n";
        }
    }
    // TODO: 进行线性扫描寄存器分配, 为每个虚拟寄存器选择合适的物理寄存器或者将其溢出到合适的栈地址中
    // 该函数中只需正确设置alloc_result，并不需要实际生成溢出代码
    // TODO("LinearScan");
    // std::cout << "普通begin:\n";
    while (!unalloc_queue.empty()) {
        auto cur_interval = unalloc_queue.top();
        unalloc_queue.pop();
        auto cur_reg = cur_interval.getReg();

        // 尝试获取空闲寄存器
        auto phy_reg = phy_regs_tools->getIdleReg(cur_interval);

        if (phy_reg >= 0) {
            // 获取成功，记录虚拟寄存器-物理寄存器的关系
            AllocPhyReg(mfun, cur_reg, phy_reg);
            current_func->AddStackSize(2);
        } else {
            spilled = true;
            int mem_off = phy_regs_tools->getIdleMem(cur_interval);
            AllocStack(mfun, cur_reg, mem_off);
        }
    }
    // std::cout << "普通end:\n";
    // 返回是否发生溢出
    return spilled;
}

// 计算溢出权重
double FastLinearScan::CalculateSpillWeight(LiveInterval interval) {
    return (double)interval.getReferenceCount() / interval.getIntervalLen();
}
