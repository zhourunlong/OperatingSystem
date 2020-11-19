package nachos.userprog;

import nachos.machine.*;
import nachos.threads.*;
import nachos.userprog.*;

import java.io.EOFException;
import java.util.HashMap;
import java.util.List;
import java.util.LinkedList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Hashtable;

/**
 * Encapsulates the state of a user process that is not contained in its
 * user thread (or threads). This includes its address translation state, a
 * file table, and information about the program being executed.
 *
 * <p>
 * This class is extended by other classes to support additional functionality
 * (such as additional syscalls).
 *
 * @see	nachos.vm.VMProcess
 * @see	nachos.network.NetProcess
 */
public class UserProcess {
    /**
     * Allocate a new process.
     */
    public UserProcess() {
        PID = ++totProcess;
        int numPhysPages = Machine.processor().getNumPhysPages();
        pageTable = new TranslationEntry[numPhysPages];
        // Machine.processor().lock_pages.acquire();
        // Machine.processor().lock_pages.release();
        // Added to support file system calls.
        // Maintain a collection of open files and free file descriptors.
        openFiles = new HashMap<Integer, OpenFile>();
        openFiles.put(0, UserKernel.console.openForReading());  // Standard input.
        openFiles.put(1, UserKernel.console.openForWriting());  // Standard output.
        freeDescriptors = new LinkedList<Integer>(Arrays.asList(2,3,4,5,6,7,8,9,10,11,12,13,14,15));

        allProc.put(PID, this);
        // System.out.println("new proc ID = " + PID + ", now " + allProc.size() + " procs");
    }

    /**
     * Allocate and return a new process of the correct class. The class name
     * is specified by the <tt>nachos.conf</tt> key
     * <tt>Kernel.processClassName</tt>.
     *
     * @return	a new process of the correct class.
     */
    public static UserProcess newUserProcess() {
        return (UserProcess)Lib.constructObject(Machine.getProcessClassName());
    }

    /**
     * Execute the specified program with the specified arguments. Attempts to
     * load the program, and then forks a thread to run it.
     *
     * @param	name	the name of the file containing the executable.
     * @param	args	the arguments to pass to the executable.
     * @return	<tt>true</tt> if the program was successfully executed.
     */
    public boolean execute(String name, String[] args) {
        if (!load(name, args)) {
            System.out.println("Fail to load " + name);
            return false;
        }

        thread = (UThread) (new UThread(this).setName(name));
        thread.fork();

        return true;
    }

    /**
     * Save the state of this process in preparation for a context switch.
     * Called by <tt>UThread.saveState()</tt>.
     */
    public void saveState() {
    }

    /**
     * Restore the state of this process after a context switch. Called by
     * <tt>UThread.restoreState()</tt>.
     */
    public void restoreState() {
        Machine.processor().setPageTable(pageTable);
    }

    /**
     * Read a null-terminated string from this process's virtual memory. Read
     * at most <tt>maxLength + 1</tt> bytes from the specified address, search
     * for the null terminator, and convert it to a <tt>java.lang.String</tt>,
     * without including the null terminator. If no null terminator is found,
     * returns <tt>null</tt>.
     *
     * @param	vaddr	the starting virtual address of the null-terminated
     *			string.
     * @param	maxLength	the maximum number of characters in the string,
     *				not including the null terminator.
     * @return	the string read, or <tt>null</tt> if no null terminator was
     *		found.
     */
    public String readVirtualMemoryString(int vaddr, int maxLength) {
        Lib.assertTrue(maxLength >= 0);

        byte[] bytes = new byte[maxLength+1];

        int bytesRead = readVirtualMemory(vaddr, bytes);

        for (int length=0; length<bytesRead; length++) {
            if (bytes[length] == 0)
                return new String(bytes, 0, length);
        }

        return null;
    }

    /**
     * Transfer data from this process's virtual memory to all of the specified
     * array. Same as <tt>readVirtualMemory(vaddr, data, 0, data.length)</tt>.
     *
     * @param	vaddr	the first byte of virtual memory to read.
     * @param	data	the array where the data will be stored.
     * @return	the number of bytes successfully transferred.
     */
    public int readVirtualMemory(int vaddr, byte[] data) {
        return readVirtualMemory(vaddr, data, 0, data.length);
    }

    /**
     * Transfer data from this process's virtual memory to the specified array.
     * This method handles address translation details. This method must
     * <i>not</i> destroy the current process if an error occurs, but instead
     * should return the number of bytes successfully copied (or zero if no
     * data could be copied).
     *
     * @param	vaddr	the first byte of virtual memory to read.
     * @param	data	the array where the data will be stored.
     * @param	offset	the first byte to write in the array.
     * @param	length	the number of bytes to transfer from virtual memory to
     *			the array.
     * @return	the number of bytes successfully transferred.
     */
    public int readVirtualMemory(int vaddr, byte[] data, int offset,
                                 int length) {
        Lib.assertTrue(offset >= 0 && length >= 0 && offset+length <= data.length);

        byte[] memory = Machine.processor().getMemory();

        int amount = 0;

        // for now, just assume that virtual addresses equal physical addresses
        if (vaddr < 0 || vaddr >= memory.length)
            return 0;

        int first_vpn = vaddr / pageSize;
        int first_pageOffset = vaddr % pageSize;

        int last_vpn = (vaddr + length - 1) / pageSize;
        int last_pageOffset = (vaddr + length - 1) % pageSize;

        if (translate(first_vpn) == -1 || translate(last_vpn) == -1)
            return 0;
        if(first_vpn > last_vpn)
            return 0;

        // Lib.assertTrue(translate(first_vpn) != -1 && translate(last_vpn) != -1);
        // Lib.assertTrue(first_vpn <= last_vpn);

        int t_ppn;

        if (last_vpn == first_vpn) {
            t_ppn = translate(first_vpn);
            if (pageTable[first_vpn].valid == false)
                return amount;
            System.arraycopy(memory, t_ppn * pageSize + first_pageOffset, data, offset, last_pageOffset - first_pageOffset + 1);
            offset += last_pageOffset - first_pageOffset + 1;
            amount += last_pageOffset - first_pageOffset + 1;
        } else {
            t_ppn = translate(first_vpn);
            if (pageTable[first_vpn].valid == false)
                return amount;
            System.arraycopy(memory, t_ppn * pageSize + first_pageOffset, data, offset, pageSize - first_pageOffset);
            offset += pageSize - first_pageOffset;
            amount += pageSize - first_pageOffset;

            for (int i = first_vpn + 1; i < last_vpn; i++) {
                t_ppn = translate(i);
                if(pageTable[i].valid == false)
                    return amount;
                System.arraycopy(memory, t_ppn * pageSize, data, offset, pageSize);
                offset += pageSize;
                amount += pageSize;
            }

            t_ppn = translate(last_vpn);
            if (pageTable[last_vpn].valid == false)
                return amount;
            System.arraycopy(memory, t_ppn * pageSize, data, offset, last_pageOffset + 1);
            amount += last_pageOffset + 1;
        }

        // amount = Math.min(length, memory.length-vaddr);
        // System.arraycopy(memory, vaddr, data, offset, amount);

        return amount;
    }

    /**
     * Transfer all data from the specified array to this process's virtual
     * memory.
     * Same as <tt>writeVirtualMemory(vaddr, data, 0, data.length)</tt>.
     *
     * @param	vaddr	the first byte of virtual memory to write.
     * @param	data	the array containing the data to transfer.
     * @return	the number of bytes successfully transferred.
     */
    public int writeVirtualMemory(int vaddr, byte[] data) {
        return writeVirtualMemory(vaddr, data, 0, data.length);
    }

    /**
     * Transfer data from the specified array to this process's virtual memory.
     * This method handles address translation details. This method must
     * <i>not</i> destroy the current process if an error occurs, but instead
     * should return the number of bytes successfully copied (or zero if no
     * data could be copied).
     *
     * @param	vaddr	the first byte of virtual memory to write.
     * @param	data	the array containing the data to transfer.
     * @param	offset	the first byte to transfer from the array.
     * @param	length	the number of bytes to transfer from the array to
     *			virtual memory.
     * @return	the number of bytes successfully transferred.
     */
    public int writeVirtualMemory(int vaddr, byte[] data, int offset,
                                  int length) {
        Lib.assertTrue(offset >= 0 && length >= 0 && offset+length <= data.length);

        byte[] memory = Machine.processor().getMemory();

        int amount = 0;

        // for now, just assume that virtual addresses equal physical addresses
        if (vaddr < 0 || vaddr >= memory.length)
            return 0;

        int first_vpn = vaddr / pageSize;
        int first_pageOffset = vaddr % pageSize;

        int last_vpn = (vaddr + length - 1) / pageSize;
        int last_pageOffset = (vaddr + length - 1) % pageSize;

        // System.out.println(first_vpn);
        // System.out.println(last_vpn);
        // System.out.println(numPages);

        if (translate(first_vpn) == -1 || translate(last_vpn) == -1)
            return 0;
        if(first_vpn > last_vpn)
            return 0;

        // Lib.assertTrue(translate(first_vpn) != -1 && translate(last_vpn) != -1);
        // Lib.assertTrue(first_vpn <= last_vpn);

        int t_ppn;

        if (last_vpn == first_vpn) {
            t_ppn = translate(first_vpn);
            // Lib.assertTrue(pageTable[first_vpn].readOnly == false);
            if(pageTable[first_vpn].valid == false || pageTable[first_vpn].readOnly == true)
                return amount;
            System.arraycopy(data, offset, memory, t_ppn * pageSize + first_pageOffset, last_pageOffset - first_pageOffset + 1);
            offset += last_pageOffset - first_pageOffset + 1;
            amount += last_pageOffset - first_pageOffset + 1;
        } else {
            t_ppn = translate(first_vpn);
            // Lib.assertTrue(pageTable[first_vpn].readOnly == false);
            if(pageTable[first_vpn].valid == false || pageTable[first_vpn].readOnly == true)
                return amount;
            System.arraycopy(data, offset, memory, t_ppn * pageSize + first_pageOffset, pageSize - first_pageOffset);
            offset += pageSize - first_pageOffset;
            amount += pageSize - first_pageOffset;

            for (int i = first_vpn + 1; i < last_vpn; i++) {
                t_ppn = translate(i);
                // Lib.assertTrue(pageTable[i].readOnly == false);
                if(pageTable[i].valid == false || pageTable[i].readOnly == true)
                    return amount;
                System.arraycopy(data, offset, memory, t_ppn * pageSize, pageSize);
                offset += pageSize;
                amount += pageSize;
            }

            t_ppn = translate(last_vpn);
            // Lib.assertTrue(pageTable[last_vpn].readOnly == false);
            if(pageTable[last_vpn].valid == false || pageTable[last_vpn].readOnly == true)
                return amount;
            System.arraycopy(data, offset, memory, t_ppn * pageSize, last_pageOffset + 1);
            amount += last_pageOffset + 1;
        }

        // int amount = Math.min(length, memory.length-vaddr);
        // System.arraycopy(data, offset, memory, vaddr, amount);

        return amount;
    }

    /**
     * Translate a virtual page number to a physical page number
     * @param vpn the virtual page number
     * @return
     */

    private int translate(int vpn) {
        for(int i = 0; i < pageTable.length; i++) {
            if(pageTable[i] != null && pageTable[i].valid && pageTable[i].vpn == vpn)
                return pageTable[i].ppn;
        }
        return -1;
    }

    /**
     * Translate a virtual page number to a physical page number
     * @param vpn the virtual page number
     * @param readOnly whether the page is read-only
     * @return
     */

    private int translate(int vpn, boolean readOnly) {
        for(int i = 0; i < pageTable.length; i++) {
            if(pageTable[i] != null && pageTable[i].valid && pageTable[i].vpn == vpn) {
                pageTable[i].readOnly = readOnly;
                return pageTable[i].ppn;
            }
        }
        return -1;
    }

    private int getNewPage() {
        UserKernel.lockForPages.acquire();

        if(UserKernel.freePages.isEmpty()) {
            handleExit(-1);
            return -1;
        }

        Lib.assertTrue(!UserKernel.freePages.isEmpty());
        int freePageNumber = UserKernel.freePages.removeFirst();

        UserKernel.lockForPages.release();
        return freePageNumber;
    }

    /**
     * Load the executable with the specified name into this process, and
     * prepare to pass it the specified arguments. Opens the executable, reads
     * its header information, and copies sections and arguments into this
     * process's virtual memory.
     *
     * @param	name	the name of the file containing the executable.
     * @param	args	the arguments to pass to the executable.
     * @return	<tt>true</tt> if the executable was successfully loaded.
     */
    private boolean load(String name, String[] args) {
        Lib.debug(dbgProcess, "UserProcess.load(\"" + name + "\")");

        OpenFile executable = ThreadedKernel.fileSystem.open(name, false);
        if (executable == null) {
            Lib.debug(dbgProcess, "\topen failed");
            return false;
        }

        try {
            coff = new Coff(executable);
        }
        catch (EOFException e) {
            executable.close();
            Lib.debug(dbgProcess, "\tcoff load failed");
            return false;
        }

        // make sure the sections are contiguous and start at page 0
        numPages = 0;
        for (int s=0; s<coff.getNumSections(); s++) {
            CoffSection section = coff.getSection(s);
            if (section.getFirstVPN() != numPages) {
                coff.close();
                Lib.debug(dbgProcess, "\tfragmented executable");
                return false;
            }
            numPages += section.getLength();
        }

        for (int i=0; i < numPages; i++) {
            // Lib.assertTrue(!UserKernel.freePages.isEmpty());
            pageTable[i] = new TranslationEntry(i, getNewPage(), true, false, false, false);
        }

        // make sure the argv array will fit in one page
        byte[][] argv = new byte[args.length][];
        int argsSize = 0;
        for (int i=0; i<args.length; i++) {
            argv[i] = args[i].getBytes();
            // 4 bytes for argv[] pointer; then string plus one for null byte
            argsSize += 4 + argv[i].length + 1;
        }
        if (argsSize > pageSize) {
            coff.close();
            Lib.debug(dbgProcess, "\targuments too long");
            return false;
        }

        // program counter initially points at the program entry point
        initialPC = coff.getEntryPoint();

        // next comes the stack; stack pointer initially points to top of it
        numPages += stackPages;

        for (int i = numPages - stackPages; i < numPages; i++) {
            // Lib.assertTrue(!UserKernel.freePages.isEmpty());
            pageTable[i] = new TranslationEntry(i, getNewPage(), true, false, false, false);
        }

        initialSP = numPages*pageSize;

        // and finally reserve 1 page for arguments
        numPages++;
        pageTable[numPages - 1] = new TranslationEntry(numPages - 1, getNewPage(), true, false, false, false);

        if (!loadSections())
            return false;

        // store arguments in last page
        int entryOffset = (numPages-1)*pageSize;
        int stringOffset = entryOffset + args.length*4;

        this.argc = args.length;
        this.argv = entryOffset;

        for (int i=0; i<argv.length; i++) {
            byte[] stringOffsetBytes = Lib.bytesFromInt(stringOffset);
            Lib.assertTrue(writeVirtualMemory(entryOffset,stringOffsetBytes) == 4);
            entryOffset += 4;
            Lib.assertTrue(writeVirtualMemory(stringOffset, argv[i]) ==
                    argv[i].length);
            stringOffset += argv[i].length;
            Lib.assertTrue(writeVirtualMemory(stringOffset,new byte[] { 0 }) == 1);
            stringOffset += 1;
        }

        // Added by Hu Yang to release file descriptor occupied by executable files.
        // WARNING: This might be wrong.
        coff.close();

        return true;
    }

    /**
     * Allocates memory for this process, and loads the COFF sections into
     * memory. If this returns successfully, the process will definitely be
     * run (this is the last step in process initialization that can fail).
     *
     * @return	<tt>true</tt> if the sections were successfully loaded.
     */
    protected boolean loadSections() {
        if (numPages > Machine.processor().getNumPhysPages()) {
            coff.close();
            Lib.debug(dbgProcess, "\tinsufficient physical memory");
            return false;
        }

        // load sections
        for (int s=0; s<coff.getNumSections(); s++) {
            CoffSection section = coff.getSection(s);

            Lib.debug(dbgProcess, "\tinitializing " + section.getName()
                    + " section (" + section.getLength() + " pages)");

            for (int i=0; i<section.getLength(); i++) {
                int vpn = section.getFirstVPN() + i;
                // System.out.println(numPages);
                // if (translate(vpn) == -1) {
                //     numPages += 1;
                //     pageTable[numPages] = new TranslationEntry(vpn, getNewPage(), true, section.isReadOnly(), false, false);
                // }
                // for now, just assume virtual addresses=physical addresses
                // System.out.println(vpn);
                // System.out.println(translate(vpn, section.isReadOnly()));
                section.loadPage(i, translate(vpn, section.isReadOnly()));
            }
        }

        return true;
    }

    /**
     * Release any resources allocated by <tt>loadSections()</tt>.
     */
    protected void unloadSections() {
        for (int i = stackPages; i < numPages; i++) {
            UserKernel.lockForPages.acquire();
            if (translate(i) != -1)
                UserKernel.freePages.add(translate(i));
            Lib.debug(dbgProcess, "Freeing page " + i);
            UserKernel.lockForPages.release();
        }
        numPages = stackPages;
    }

    /**
     * Initialize the processor's registers in preparation for running the
     * program loaded into this process. Set the PC register to point at the
     * start function, set the stack pointer register to point at the top of
     * the stack, set the A0 and A1 registers to argc and argv, respectively,
     * and initialize all other registers to 0.
     */
    public void initRegisters() {
        Processor processor = Machine.processor();

        // by default, everything's 0
        for (int i=0; i<processor.numUserRegisters; i++)
            processor.writeRegister(i, 0);

        // initialize PC and SP according
        processor.writeRegister(Processor.regPC, initialPC);
        processor.writeRegister(Processor.regSP, initialSP);

        // initialize the first two argument registers to argc and argv
        processor.writeRegister(Processor.regA0, argc);
        processor.writeRegister(Processor.regA1, argv);
    }

    /**
     * Handle the halt() system call.
     */
    private int handleHalt() {

        if (PID != 1) {
            System.out.println("Halt: not root calling!");
            return -1;
        }
        Machine.halt();

        Lib.assertNotReached("Machine.halt() did not halt machine!");
        return 0;
    }

    /*************************************************************************
     * The following system calls are implemented by Hu Yang (Problem #1).
     *************************************************************************/

    /** Syscall prototype: int creat(char *name);
     * @param   filenameAddr: char*, pointer to the filename string in virtual memory.
     * @return  A new file descriptor (0~15), or an error flag (-1).
     **/
    private int handleCreate(int filenameAddr) {
        Lib.debug(dbgProcess, "===== Handling Create =====");
        String filename = readVirtualMemoryString(filenameAddr, 255);
        if (filename == null) {
            System.out.println("handleCreate Error: Fail to retrieve file name.");
            return -1;  // If filename does not exist at given address, return -1.
        }

        OpenFile file = ThreadedKernel.fileSystem.open(filename, true);  // Overwrite on collision.
        if (file == null) {
            System.out.println("handleCreate Error: Fail to open the created file.");
            return -1;  // If file system fails to open the file (given filename), return -1.
        }

        if (freeDescriptors.isEmpty()) {
            file.close();
            System.out.println("handleCreate Error: No available file descriptors.");
            return -1;  // If there are no available file descriptors, return -1.
        }

        Integer newFileDescriptor = freeDescriptors.remove(0);
        openFiles.put(newFileDescriptor, file);
        return newFileDescriptor;  // On success, return a file descriptor.
    }

    /** Syscall prototype: int open(char *name);
     * @param   filenameAddr: char*, pointer to the filename string in virtual memory.
     * @return  A new file descriptor (0~15), or an error flag (-1).
     **/
    private int handleOpen(int filenameAddr) {
        Lib.debug(dbgProcess, "===== Handling Open =====");
        String filename = readVirtualMemoryString(filenameAddr, 255);
        if (filename == null) {
            System.out.println("handleOpen Error: Fail to retrieve file name.");
            return -1;  // If filename does not exist at given address, return -1.
        }

        OpenFile file = ThreadedKernel.fileSystem.open(filename, false);  // Error on file miss.
        if (file == null) {
            System.out.println("handleOpen Error: Fail to open the file.");
            return -1;  // If file system fails to open the file (given filename), return -1.
        }

        if (freeDescriptors.isEmpty()) {
            file.close();
            System.out.println("handleOpen Error: No available file descriptors.");
            return -1;  // If there are no available file descriptors, return -1.
        }

        Integer newFileDescriptor = freeDescriptors.remove(0);
        openFiles.put(newFileDescriptor, file);
        return newFileDescriptor;  // On success, return a file descriptor.
    }

    /** Syscall prototype: int read(int fd, char *buffer, int size);
     * @param   fileDescriptor: int (0~15), file descriptor number (local to the current process).
     * @param   readBufferAddr: char*, pointer to the read buffer in virtual memory.
     * @param   maxReadCount: int, the maximum number of bytes to be read.
     * @return  The number of bytes read from the file <fileDescriptor>.
     * For disk files, we should also advance current file position.
     * Error occurs if fileDescriptor or buffer is invalid (return < maxCount is not an error).
     **/
    private int handleRead(int fileDescriptor, int readBufferAddr, int maxReadCount) {
        Lib.debug(dbgProcess, "===== Handling Read =====");
        OpenFile file = openFiles.get(fileDescriptor);
        if (file == null) {
            System.out.println("handleRead Error: File descriptor does not exist.");
            return -1;  // If file descriptor is not in use, return -1.
        }

        // Read from readBuffer.
        byte[] buffer = new byte[maxReadCount];
        int readFromBuffer = file.read(buffer, 0, maxReadCount);
        if (readFromBuffer == -1) {
            System.out.println("handleRead Error: Exception during reading readBuffer.");
            return -1;  // If an exception occurs during reading the readBuffer, return -1.
        }

        // Write to virtual memory.
        int writtenToMemory = writeVirtualMemory(readBufferAddr, buffer, 0, readFromBuffer);
        if (writtenToMemory == -1) {
            System.out.println("handleRead Error: Exception during writing memory.");
            return -1;  // If an exception occurs during writing the memory, return -1.
        }
        if (writtenToMemory != readFromBuffer) {
            System.out.println("handleRead Error: Not enough bytes written to memory.");
            return -1;  // If not enough bytes are written to the memory, return -1.
        }

        return writtenToMemory;
    }

    /** Syscall prototype: int write(int fd, char *buffer, int size);
     * @param   fileDescriptor: int (0~15), file descriptor number (local to the current process).
     * @param   writeBufferAddr: char*, pointer to the read buffer in virtual memory.
     * @param   writeCount: int, the maximum number of bytes to be written.
     * @return  The number of bytes written to the file <fileDescriptor>.
     * For disk files, we should also advance current file position.
     * Error occurs if fileDescriptor or buffer is invalid, or return < maxCount (caution here).
     **/
    private int handleWrite(int fileDescriptor, int writeBufferAddr, int writeCount) {
        Lib.debug(dbgProcess, "===== Handling Write =====");
        OpenFile file = openFiles.get(fileDescriptor);
        if (file == null) {
            System.out.println("handleWrite Error: File descriptor does not exist.");
            return -1;  // If file descriptor is not in use, return -1.
        }

        // Read from writeBuffer.
        byte[] buffer = new byte[writeCount];
        int readFromBuffer = readVirtualMemory(writeBufferAddr, buffer, 0, writeCount);
        if (readFromBuffer == -1) {
            System.out.println("handleWrite Error: Exception during reading writeBuffer.");
            return -1;  // If an exception occurs during reading the writeBuffer, return -1.
        }
        if (readFromBuffer != writeCount) {
            System.out.println("handleWrite Error: Not enough bytes read from writeBuffer.");
            return -1;  // If not enough bytes are read from the writeBuffer, return -1.
        }

        // Write to file.
        int writtenToFile = file.write(buffer, 0, writeCount);
        if (writtenToFile == -1) {
            System.out.println("handleWrite Error: Exception during writing file.");
            return -1;  // If an exception occurs during writing the file, return -1.
        }
        if (writtenToFile != writeCount) {
            System.out.println("handleWrite Error: Not enough bytes written to file.");
            return -1;  // If not enough bytes are written to the file, return -1.
        }

        return writtenToFile;
    }

    /** Syscall prototype: int close(int fd);
     * @param   fileDescriptor: int (0~15), file descriptor number (local to the current process).
     * @return  0 on success, and -1 on failure.
     **/
    private int handleClose(int fileDescriptor) {
        Lib.debug(dbgProcess, "===== Handling Close =====");
        OpenFile file = openFiles.get(fileDescriptor);
        if (file == null) {
            System.out.println("handleClose Error: File descriptor does not exist.");
            return -1;  // If file descriptor is not in use, return -1.
        }

        file.close();
        openFiles.remove(fileDescriptor);
        freeDescriptors.add(fileDescriptor);
        return 0;
    }

    /** Syscall prototype: int unlink(char *name);
     * @param   filenameAddr: char*, pointer to the filename string in virtual memory.
     * @return  0 on success, and -1 on failure.
     * This syscall removes a specific file from the file system.
     * No change happens to the thread state.
     **/
    private int handleUnlink(int filenameAddr) {
        Lib.debug(dbgProcess, "===== Handling Unlink =====");
        String filename = readVirtualMemoryString(filenameAddr, 255);
        if (filename == null) {
            System.out.println("handleUnlink Error: File descriptor does not exist.");
            return -1;  // If filename does not exist at given address, return -1.
        }

        // Try to remove file, and return success flag.
        return ThreadedKernel.fileSystem.remove(filename) ? 0 : -1;
    }

    /*************************************************************************
     * The above system calls are implemented by Hu Yang (Problem #1).
     *************************************************************************/

    // Begin Task 3

    private int handleExec(int filePtr, int argc, int argvPtr) {
        Lib.debug(dbgProcess, "===== Handling Exec =====");
        String fileName = readVirtualMemoryString(filePtr, 255);
        if (fileName == null) {
            System.out.println("Exec: file not found");
            return -1;
        }
        if (!fileName.endsWith(".coff")) {
            System.out.println("Exec: not end with .coff");
            return -1;
        }
        if (argc < 0) {
            System.out.println("Exec: argc < 0");
            return -1;
        }

        String[] argv = new String[argc];
        byte[] buf = new byte[4 * argc];
        if (readVirtualMemory(argvPtr, buf) != 4 * argc) {
            System.out.println("Exec: argv length error");
            return -1;
        }

        for (int i = 0; i < argc; ++i) {
            argv[i] = readVirtualMemoryString(Lib.bytesToInt(buf, 4 * i), 255);
            //System.out.println(argv[i]);
            if (argv[i] == null) {
                System.out.println("Exec: argv[" + i + "] not found");
                return -1;
            }
        }

        UserProcess proc = newUserProcess();
        childProc.add(proc.PID);

        if (!proc.execute(fileName, argv)) {
            System.out.println("Exec: execute error!");
            allProc.remove(proc.PID);
            return -1;
        }

        return proc.PID;
    }

    private int handleJoin(int processID, int statusPtr) {
        Lib.debug(dbgProcess, "===== Handling Join =====");

        //System.out.println("Join: " + processID);

        if (!childProc.contains(processID)) {
            System.out.println("Join: not child");
            return -1;
        }

        UserProcess proc = allProc.get(processID);
        if (proc == null) {
            System.out.println("Join: child already exited or child not exist");
            return 0;
        }
        childProc.remove(processID);
        proc.thread.join();
        
        byte[] buf = new byte[4];
        buf = Lib.bytesFromInt(proc.status);
        if (proc.status == -1 || writeVirtualMemory(statusPtr, buf) != 4) {
            System.out.println("Join: child exited abnormally");
            return 0;
        } else
            return 1;
    }

    private int handleExit(int _status) {
        Lib.debug(dbgProcess, "===== Handling Exit =====");
        status = _status;

        System.out.println(PID + " closing files");

        unloadSections();
        
        childProc.clear();
        allProc.remove(PID);

        for (int i = 0; i < numPages; i++) {
            UserKernel.lockForPages.acquire();
            if (translate(i) != -1)
                UserKernel.freePages.add(translate(i));
            Lib.debug(dbgProcess, "Freeing page " + pageTable[i].ppn);
            UserKernel.lockForPages.release();
        }

        System.out.println(PID + " exited, remain " + allProc.size() + " procs");

        if (allProc.size() == 0)
            Kernel.kernel.terminate();
        else
            UThread.finish();

        return 0;
    }

    // End Task 3

    private static final int
            syscallHalt = 0,
            syscallExit = 1,
            syscallExec = 2,
            syscallJoin = 3,
            syscallCreate = 4,
            syscallOpen = 5,
            syscallRead = 6,
            syscallWrite = 7,
            syscallClose = 8,
            syscallUnlink = 9;

    /**
     * Handle a syscall exception. Called by <tt>handleException()</tt>. The
     * <i>syscall</i> argument identifies which syscall the user executed:
     *
     * <table>
     * <tr><td>syscall#</td><td>syscall prototype</td></tr>
     * <tr><td>0</td><td><tt>void halt();</tt></td></tr>
     * <tr><td>1</td><td><tt>void exit(int status);</tt></td></tr>
     * <tr><td>2</td><td><tt>int  exec(char *name, int argc, char **argv);
     * 								</tt></td></tr>
     * <tr><td>3</td><td><tt>int  join(int pid, int *status);</tt></td></tr>
     * <tr><td>4</td><td><tt>int  creat(char *name);</tt></td></tr>
     * <tr><td>5</td><td><tt>int  open(char *name);</tt></td></tr>
     * <tr><td>6</td><td><tt>int  read(int fd, char *buffer, int size);
     *								</tt></td></tr>
     * <tr><td>7</td><td><tt>int  write(int fd, char *buffer, int size);
     *								</tt></td></tr>
     * <tr><td>8</td><td><tt>int  close(int fd);</tt></td></tr>
     * <tr><td>9</td><td><tt>int  unlink(char *name);</tt></td></tr>
     * </table>
     *
     * @param	syscall	the syscall number.
     * @param	a0	the first syscall argument.
     * @param	a1	the second syscall argument.
     * @param	a2	the third syscall argument.
     * @param	a3	the fourth syscall argument.
     * @return	the value to be returned to the user.
     */
    public int handleSyscall(int syscall, int a0, int a1, int a2, int a3) {
        switch (syscall) {
            case syscallHalt:
                return handleHalt();

            // The following system calls are implemented by Hu Yang (Problem #1).
            case syscallCreate:
                return handleCreate(a0);
            case syscallOpen:
                return handleOpen(a0);
            case syscallRead:
                return handleRead(a0, a1, a2);
            case syscallWrite:
                return handleWrite(a0, a1, a2);
            case syscallClose:
                return handleClose(a0);
            case syscallUnlink:
                return handleUnlink(a0);

            // Task 3
            case syscallExec:
                return handleExec(a0, a1, a2);
            case syscallJoin:
                return handleJoin(a0, a1);
            case syscallExit:
                return handleExit(a0);

            default:
                Lib.debug(dbgProcess, "Unknown syscall " + syscall);
                Lib.assertNotReached("Unknown system call!");
        }
        return 0;
    }

    /**
     * Handle a user exception. Called by
     * <tt>UserKernel.exceptionHandler()</tt>. The
     * <i>cause</i> argument identifies which exception occurred; see the
     * <tt>Processor.exceptionZZZ</tt> constants.
     *
     * @param	cause	the user exception that occurred.
     */
    public void handleException(int cause) {
        Processor processor = Machine.processor();

        switch (cause) {
            case Processor.exceptionSyscall:
                int result = handleSyscall(processor.readRegister(Processor.regV0),
                        processor.readRegister(Processor.regA0),
                        processor.readRegister(Processor.regA1),
                        processor.readRegister(Processor.regA2),
                        processor.readRegister(Processor.regA3)
                );
                processor.writeRegister(Processor.regV0, result);
                processor.advancePC();
                break;

            default:
                Lib.debug(dbgProcess, "Unexpected exception: " +
                        Processor.exceptionNames[cause]);
                Lib.assertNotReached("Unexpected exception");
        }
    }

    /** The program being run by this process. */
    protected Coff coff;

    /** This process's page table. */
    protected TranslationEntry[] pageTable;
    /** The number of contiguous pages occupied by the program. */
    protected int numPages;

    /** The number of pages in the program's stack. */
    protected final int stackPages = 8;

    private int initialPC, initialSP;
    private int argc, argv;

    private static final int pageSize = Processor.pageSize;
    private static final char dbgProcess = 'a';

    private HashMap<Integer, OpenFile> openFiles;
    private List<Integer> freeDescriptors;

    // thread
    private UThread thread;

    // status
    private int status = -1;

    // record PIDs
    private int PID;
    private HashSet<Integer> childProc = new HashSet<Integer>();
    private static int totProcess = 0;
    private static Hashtable<Integer, UserProcess> allProc =
        new Hashtable<Integer, UserProcess>();
}
