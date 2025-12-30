#!/usr/bin/env python3
"""
MiniOB 功能调试测试脚本
用于系统化测试所有实现的功能
"""

import subprocess
import sys
import os
import time
from datetime import datetime
from pathlib import Path

class Colors:
    """终端颜色"""
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    RESET = '\033[0m'
    BOLD = '\033[1m'

class TestResult:
    """测试结果"""
    def __init__(self, stage_name):
        self.stage_name = stage_name
        self.passed = 0
        self.failed = 0
        self.errors = []
        self.start_time = None
        self.end_time = None

    def duration(self):
        if self.start_time and self.end_time:
            return self.end_time - self.start_time
        return 0

class MiniOBTester:
    """MiniOB 测试器"""
    
    def __init__(self, observer_path, socket_path):
        self.observer_path = observer_path
        self.socket_path = socket_path
        self.observer_process = None
        self.results = []
        
        # 测试阶段定义
        self.test_stages = [
            {
                'name': '阶段1: 基础功能',
                'file': 'test_plan_stage1_basic.sql',
                'features': ['basic', 'select-meta', 'drop-table'],
                'critical': True
            },
            {
                'name': '阶段2: 类型扩展',
                'file': 'test_plan_stage2_types.sql',
                'features': ['date', 'text', 'null'],
                'critical': True
            },
            {
                'name': '阶段3: 索引功能',
                'file': 'test_plan_stage3_index.sql',
                'features': ['multi-index', 'unique'],
                'critical': False
            },
            {
                'name': '阶段4: 查询增强',
                'file': 'test_plan_stage4_query.sql',
                'features': ['select-tables', 'join-tables', 'order-by', 'group-by', 'aggregation-func'],
                'critical': False
            },
            {
                'name': '阶段5: 高级功能',
                'file': 'test_plan_stage5_advanced.sql',
                'features': ['simple-sub-query', 'update', 'insert(多行)'],
                'critical': False
            },
            {
                'name': '回归测试',
                'file': 'test_plan_regression.sql',
                'features': ['综合测试'],
                'critical': False
            }
        ]

    def print_header(self, text):
        """打印标题"""
        print(f"\n{Colors.BOLD}{Colors.BLUE}{'='*60}{Colors.RESET}")
        print(f"{Colors.BOLD}{Colors.BLUE}{text:^60}{Colors.RESET}")
        print(f"{Colors.BOLD}{Colors.BLUE}{'='*60}{Colors.RESET}\n")

    def print_success(self, text):
        """打印成功信息"""
        print(f"{Colors.GREEN}✓ {text}{Colors.RESET}")

    def print_error(self, text):
        """打印错误信息"""
        print(f"{Colors.RED}✗ {text}{Colors.RESET}")

    def print_warning(self, text):
        """打印警告信息"""
        print(f"{Colors.YELLOW}⚠ {text}{Colors.RESET}")

    def start_observer(self):
        """启动 observer 服务"""
        print("启动 MiniOB observer 服务...")
        
        # 删除旧的socket文件
        if os.path.exists(self.socket_path):
            os.remove(self.socket_path)
        
        try:
            self.observer_process = subprocess.Popen(
                [self.observer_path, '-s', self.socket_path],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            
            # 等待服务启动
            time.sleep(2)
            
            if self.observer_process.poll() is not None:
                self.print_error("observer 启动失败")
                return False
                
            self.print_success("observer 服务已启动")
            return True
            
        except Exception as e:
            self.print_error(f"启动 observer 失败: {e}")
            return False

    def stop_observer(self):
        """停止 observer 服务"""
        if self.observer_process:
            print("\n停止 observer 服务...")
            self.observer_process.terminate()
            try:
                self.observer_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.observer_process.kill()
            self.print_success("observer 服务已停止")

    def run_sql_file(self, sql_file):
        """运行 SQL 文件"""
        if not os.path.exists(sql_file):
            self.print_error(f"测试文件不存在: {sql_file}")
            return False
        
        print(f"\n运行测试文件: {sql_file}")
        
        # 这里简化处理，实际应该通过客户端执行SQL
        # 可以使用 obclient 或者直接socket通信
        
        self.print_warning("注意: 自动执行功能需要实现客户端通信")
        self.print_warning("建议手动在 CLI 模式下运行测试文件中的 SQL")
        
        return True

    def run_stage(self, stage):
        """运行测试阶段"""
        self.print_header(stage['name'])
        
        print(f"测试功能: {', '.join(stage['features'])}")
        print(f"测试文件: {stage['file']}")
        print(f"关键测试: {'是' if stage['critical'] else '否'}\n")
        
        result = TestResult(stage['name'])
        result.start_time = time.time()
        
        # 运行测试
        success = self.run_sql_file(stage['file'])
        
        result.end_time = time.time()
        
        if success:
            self.print_success(f"{stage['name']} 完成")
        else:
            self.print_error(f"{stage['name']} 失败")
            if stage['critical']:
                self.print_error("这是关键测试，建议先修复再继续")
        
        self.results.append(result)
        return success

    def generate_report(self):
        """生成测试报告"""
        self.print_header("测试报告")
        
        print(f"测试时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        
        total_stages = len(self.results)
        passed_stages = sum(1 for r in self.results if r.passed > r.failed)
        
        print(f"总测试阶段: {total_stages}")
        print(f"通过阶段: {passed_stages}")
        print(f"失败阶段: {total_stages - passed_stages}\n")
        
        for result in self.results:
            status = "✓" if result.passed > result.failed else "✗"
            print(f"{status} {result.stage_name} - 耗时: {result.duration():.2f}s")
        
        print("\n" + "="*60)

    def run_all_tests(self):
        """运行所有测试"""
        self.print_header("MiniOB 功能调试测试")
        
        print("本测试脚本包含以下阶段:\n")
        for i, stage in enumerate(self.test_stages, 1):
            print(f"{i}. {stage['name']}")
            print(f"   功能: {', '.join(stage['features'])}")
            print(f"   文件: {stage['file']}\n")
        
        # 由于需要手动运行，给出指导
        self.print_header("测试方法")
        print("推荐使用以下方式进行测试:\n")
        
        print("方法1: F5 调试模式（推荐）")
        print("  1. 在 VSCode 中按 F5 启动 observer")
        print("  2. 在 DEBUG CONSOLE 中逐个运行测试文件的 SQL")
        print("  3. 观察输出结果，检查是否符合预期\n")
        
        print("方法2: CLI 模式")
        print("  1. 启动: ./bin/observer -f ../etc/observer.ini -P cli")
        print("  2. 复制粘贴测试文件中的 SQL 命令")
        print("  3. 检查输出结果\n")
        
        print("方法3: Unix Socket 模式")
        print("  1. 启动: ./bin/observer -f ../etc/observer.ini -s miniob.sock")
        print("  2. 客户端: ./bin/obclient -s miniob.sock")
        print("  3. 在客户端中运行 SQL\n")
        
        self.print_header("测试顺序建议")
        print("按照以下顺序测试，每个阶段通过后再进行下一阶段:\n")
        
        for i, stage in enumerate(self.test_stages, 1):
            critical = " [关键]" if stage['critical'] else ""
            print(f"{i}. {stage['name']}{critical}")
            print(f"   文件: {stage['file']}\n")

def main():
    """主函数"""
    print(f"{Colors.BOLD}MiniOB 功能调试测试工具{Colors.RESET}\n")
    
    # 检查必要文件
    required_files = [
        'test_plan_stage1_basic.sql',
        'test_plan_stage2_types.sql',
        'test_plan_stage3_index.sql',
        'test_plan_stage4_query.sql',
        'test_plan_stage5_advanced.sql',
        'test_plan_regression.sql'
    ]
    
    missing_files = [f for f in required_files if not os.path.exists(f)]
    
    if missing_files:
        print(f"{Colors.RED}缺少测试文件:{Colors.RESET}")
        for f in missing_files:
            print(f"  - {f}")
        print("\n请确保所有测试文件都存在。")
        return 1
    
    print(f"{Colors.GREEN}✓ 所有测试文件已就绪{Colors.RESET}\n")
    
    # 创建测试器
    tester = MiniOBTester(
        observer_path='./build_debug/bin/observer',
        socket_path='miniob.sock'
    )
    
    # 运行测试
    tester.run_all_tests()
    
    return 0

if __name__ == '__main__':
    sys.exit(main())

