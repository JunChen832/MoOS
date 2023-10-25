#include "interrupts.h"
#include "types.h"
#include "textio.h"
#include "port.h"


InterruptHandler::InterruptHandler(uint8_t interruptNumber, InterruptManager* interruptManager) {
    this->interruptNumber = interruptNumber;
    this->interruptManager = interruptManager;
    interruptManager->handlers[interruptNumber] = this;
}

InterruptHandler::~InterruptHandler() {
    if(this->interruptManager->handlers[interruptNumber] == this)
        this->interruptManager->handlers[interruptNumber] = 0;
}

uint32_t InterruptHandler::handleInterrupt(uint32_t esp) {
    puts("Gotta handle this!");
    return esp;
}





// 静态变量IDT在头文件中声明，在.cpp文件中定义
InterruptManager::GateDescriptor InterruptManager::interruptDescriptorTable[256];

InterruptManager* InterruptManager::activeInterruptManager = 0;

// 按位设置门描述符
void InterruptManager::setInterruptDescriptorTableEntry (
    uint8_t interruptNumber,
    uint16_t codeSegmentSelectorOffset,
    void (*handler)(),
    uint8_t DescriptorPrivilegeLevel,
    uint8_t DescriptorType
) {

    const uint8_t IDT_DESC_PRESENT = 0x80;

    interruptDescriptorTable[interruptNumber].handlerAdressLowBits = ((uint32_t) handler) & 0xFFFF;
    interruptDescriptorTable[interruptNumber].handlerAdressHighBits = (((uint32_t) handler) >> 16) & 0xFFFF;
    interruptDescriptorTable[interruptNumber].gdt_codeSegmentSelector = codeSegmentSelectorOffset;
    interruptDescriptorTable[interruptNumber].access = IDT_DESC_PRESENT | DescriptorType | ((DescriptorPrivilegeLevel & 3) << 5); 
    interruptDescriptorTable[interruptNumber].reserved = 0;

}


InterruptManager::InterruptManager(GlobalDescriptorTable* gdt) :
    masterPicCommand(0x20),
    masterPicData(0x21),
    slavePicCommand(0xa0),
    slavePicData(0xa1)
{
	//由代码段选择子得到段偏移
    uint16_t CodeSegment = gdt -> CodeSegmentSelector();
    const uint8_t IDT_INTERRUPT_GATE = 0xE;

	//初始化256个异常,其中中断服务例程的入口地址暂时设为&interruptIgnore
    for(int i = 0; i < 256; i++) {
        handlers[i] = 0;
        setInterruptDescriptorTableEntry(i, CodeSegment, &ignoreInterruptRequest, 0, IDT_INTERRUPT_GATE);
    }
    
    setInterruptDescriptorTableEntry(0x20, CodeSegment, &handleInterruptRequest0x00, 0, IDT_INTERRUPT_GATE);
    setInterruptDescriptorTableEntry(0x21, CodeSegment, &handleInterruptRequest0x01, 0, IDT_INTERRUPT_GATE);
    setInterruptDescriptorTableEntry(0x2C, CodeSegment, &handleInterruptRequest0x0C, 0, IDT_INTERRUPT_GATE);

/*-------------------------------------------------------------------------
下面一块内容与硬件编程相关，中断控制器需要进行一些初始化操作。中断控制器芯片是8259A，
一个控制器能够控制8个引脚，最多实现8个中断。
8259A通过两个I/O地址来进行中断相关的数据传送，对于单个的8259A或者是两级级联中的主8259A而言，
这两个I/O地址是0x20和0x21。对于两级级联的从8259A而言，这两个I/O地址是0xA0和0xA1。
我们需要在中断管理类中定义这四个端口并初始化。  
-------------------------------------------------------------------------*/
    masterPicCommand.write(0x11);
    slavePicCommand.write(0x11);

    masterPicData.write(0x20);
    slavePicData.write(0x28);

    masterPicData.write(0x04);
    slavePicData.write(0x02);

    masterPicData.write(0x01);
    slavePicData.write(0x01);

    masterPicData.write(0x00);
    slavePicData.write(0x00);

    interruptDescriptorTablePointer idt;
    idt.size = 256 * sizeof(GateDescriptor) - 1;
    idt.base = (uint32_t) interruptDescriptorTable;

	//和GDT一样，IDT的地址也放在一个特殊的寄存器idtr中，使用lidt指令进行装载。
	//我们在类中创建一个符合这个寄存器结构的结构体。它的结构和装载GDT的寄存器类似。
    asm volatile("lidt %0" : : "m" (idt));
}

InterruptManager::~InterruptManager() {}


uint32_t InterruptManager::doHandleInterrupt(uint8_t interruptNumber, uint32_t esp) {

    if(this->handlers[interruptNumber] != 0) {

        esp = this->handlers[interruptNumber]->handleInterrupt(esp);
    
    } else if (interruptNumber != 0x20) {
    
        print("No handler for interrupt: ", 0x0a);
        print("0x", 0x0b);
        print_int(interruptNumber, 0x0b, 16);
        print("\n");
    }

    //Is this a hardware interrupt
    if(0x20 <= interruptNumber && interruptNumber < 0x30) {
        //answer the interrupt
        masterPicCommand.write(0x20);
        if(0x28 <= interruptNumber)
            slavePicCommand.write(0x20);
    }

    return esp;
}

uint32_t InterruptManager::handleInterrupt(uint8_t interruptNumber, uint32_t esp) {
    if(activeInterruptManager != 0)
        return activeInterruptManager->doHandleInterrupt(interruptNumber, esp);
    return esp;
}


//CPU开启中断也需要一个特殊的指令sti，我们用类方法activate实现它.
void InterruptManager::activate() {
    if(activeInterruptManager != 0)
        activeInterruptManager->deactivate();
    activeInterruptManager = this;
    asm("sti");
}

void InterruptManager::deactivate() {
    if(activeInterruptManager == this) {
        activeInterruptManager = 0;
        asm("cli");
    }
}