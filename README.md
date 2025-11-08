# OFS Project
## Phase 1: Multi-User File System

### Project Overview
You will build a multi-user file system called OFS (Omni File System) that stores all data in a single .omni container file. Multiple users can connect simultaneously through a socket-based server, and all operations are processed sequentially to ensure data consistency.

**Real-World Analogy:** Think of a bank where multiple tellers serve customers. Each transaction must complete fully before the next one begins, ensuring account balances are always accurate and consistent.

### Rough System Architecture
```
           │
┌──────────┴──────────┐
│   Socket Server     │
│   (Port XXXX)       │
└──────────┬──────────┘
           │
┌──────────┴──────────┐
│  FIFO Queue System  │
│  (Sequential)       │
└──────────┬──────────┘
           │
┌──────────┴──────────┐
│  Your Core Logic    │
│  (File Operations)  │
└──────────┬──────────┘
           │
┌──────────┴──────────┐
│   student_id.omni   │
│   (Binary File)     │
└─────────────────────┘
```

### Standard Types and Structures
**CRITICAL:** All students must use these exact type definitions to ensure UI portability. This allows any UI to work with any backend implementation.

**Header File (ofs_types.h)**
```c
// Standard type definitions would go here
```

### Core Requirements

#### 1. File System Container (.omni)
All data must be stored in a single binary file with .omni extension. You must design:
- File header structure - Use the standard OMNIHeader above
- User table - Store UserInfo structures
- File/directory entries - Store file metadata and content
- Data blocks - Actual file content storage

**Important:** You must implement all data structures yourself. Do not use existing file system libraries.

#### 2. Configuration File (.uconf)
Create a configuration file that defines:
```ini
[filesystem]
total_size = 104857600        # Total size in bytes (100MB)
header_size = 512             # Header size (must match OMNIHeader)
block_size = 4096             # Block size (4KB recommended)
max_files = 1000              # Maximum number of files
max_filename_length = 010     # Maximum filename length

[security]
max_users = 50                # Maximum number of users
admin_username = "admin"      # Default admin username
admin_password = "admin123"   # Default admin password
require_auth = true           # Require authentication

[server]
port = 8080                   # Server port
max_connections = 20          # Maximum simultaneous connections
queue_timeout = 30            # Maximum queue wait time (seconds)
```

#### 3. Socket-Based Server
Build a TCP socket server that:
- Listens on port 8080 (or configured port)
- Accepts multiple client connections simultaneously
- Receives JSON-formatted requests
- Processes requests in FIFO (First-In-First-Out) order
- Sends JSON-formatted responses

#### 4. FIFO Operation Queue
Implement a queue system where:
- All incoming requests are added to a queue
- Operations are processed one at a time in the order received
- No two operations run simultaneously
- Each operation completes fully before the next begins

**Why FIFO?** This simple approach ensures data consistency without complex locking mechanisms. Since operations never overlap, you avoid race conditions and file corruption.

### Critical Data Structure Decisions

#### Loading and Indexing the File System
When your server starts (fs_init), you must load the entire file system into memory for fast access. This is where data structure choice becomes critical.

#### Challenge 1: Loading Users from Disk
When you call fs_init, you need to:
- Read the user table from the .omni file (array of UserInfo structures)
- Load all users into memory
- Enable fast lookup during login operations

**Think about:**
- What data structure would allow O(1) or O(log n) user lookup by username?
- How will you store users in memory after reading them from disk?
- Should you keep users sorted? Hashed? In a tree structure?
- How will you handle the "admin checks permission" scenario where you need to verify if a user exists quickly?

**Example scenario to consider:**
When user_login("john_doe", "password123") is called:
1. You need to find john_doe in your loaded user structure
2. Verify password hash
3. Create session

If you have 50 users, how quickly can you find john_doe?
Linear search (O(n))? Or something faster?

#### Challenge 2: File and Directory Indexing
After loading users, you must load the file system structure:

**Think about:**
- How will you represent the directory tree in memory?
- What structure allows fast path lookup? (e.g., /accounts/savings/john.txt)
- How will you handle listing all files in a directory efficiently?
- Should you use a tree? A graph? Multiple structures working together?

**Example scenario to consider:**
When dir_list("/accounts") is called:
- Need to find the "accounts" directory
- List all its children (files and subdirectories)
- Return them as an array of FileEntry structures

What structure makes this efficient?

#### Challenge 3: Fast File Lookup
When a file operation occurs (create, read, delete), you need instant access to file metadata:

**Think about:**
- How will you map file paths to their location in the .omni file?
- What structure enables O(1) lookup of file metadata by path?
- How will you track free blocks in the file system?
- Should you maintain an index? A table? Multiple indices?

**Example scenario to consider:**
When file_read("/reports/daily.txt") is called:
1. Find the file metadata (where is it in .omni?)
2. Read the actual data blocks
3. Return content to user

Fast lookup of "/reports/daily.txt" → block location is essential

#### Challenge 4: Free Space Management
As files are created and deleted, you need to track available space:

**Think about:**
- How will you know which blocks in the .omni file are free?
- What structure allows quick allocation of N consecutive blocks?
- How will you handle fragmentation over time?
- Should you use a bitmap? A free list? A tree of free regions?

**Example scenario to consider:**
When file_create needs 10 blocks:
- Find 10 free blocks quickly
- Mark them as used
- Update your free space structure

What structure makes finding N free blocks fast?

### File System Loading Strategy

#### The fs_init Process
When your system starts, you must efficiently load everything into memory:

```c
int fs_init(void** instance, const char* omni_path, const char* config_path) {
    // Step 1: Read configuration
    // Think about: How do you parse and store config values?
    
    // Step 2: Open .omni file
    // Think about: Keep file handle open or open/close per operation?
    
    // Step 3: Read and validate header (OMNIHeader structure)
    // Think about: What if header is corrupted? Check magic number?
    
    // Step 4: Load user table into memory
    // CRITICAL: What data structure do you use here?
    // - Need fast lookup by username
    // - Need to iterate all users (for admin list operation)
    // - Need to add/remove users dynamically
    // - Users are stored as UserInfo structures on disk
    
    // Step 5: Build file system index
    // CRITICAL: What structure represents your directory tree?
    // - Need to traverse paths efficiently
    // - Need to list directory contents (return FileEntry arrays)
    // - Need to find files by full path quickly
    
    // Step 6: Build free space index
    // CRITICAL: How do you track available blocks?
    // - Need to allocate space quickly
    // - Need to free space when files are deleted
    // - Need to report total free space
    
    // Step 7: Create instance object
    // This holds all your in-memory structures
    
    return OFS_SUCCESS;
}
```

#### Key Questions to Answer in Your Design

**User Structure Loading:**
- Do you load all users at startup or on-demand?
- What structure allows both fast lookup AND iteration?
- How do you handle user updates (add/delete)?

**Directory Tree Loading:**
- Do you load the entire tree or build it lazily?
- How do you represent parent-child relationships?
- What enables fast path traversal (e.g., /a/b/c/file.txt)?

**File Metadata Indexing:**
- Do you create an in-memory index of all files?
- How do you map paths to physical locations in .omni?
- What structure minimizes lookup time?

**Memory vs. Disk Trade-offs:**
- What stays in memory vs. what's read from disk per operation?
- How do you balance memory usage with performance?
- When do you write changes back to disk?

### Socket Communication Protocol

#### Message Format
All messages are JSON objects sent over TCP sockets.

**Request Format:**
```json
{
  "operation": "operation_name",
  "session_id": "user_session_id",
  "parameters": {
    "param1": "value1",
    "param2": "value2"
  },
  "request_id": "unique_request_id"
}
```

**Response Format:**
```json
{
  "status": "success",
  "operation": "operation_name",
  "request_id": "unique_request_id",
  "data": {
    "result_data": "value"
  }
}
```

**Error Response:**
```json
{
  "status": "error",
  "operation": "operation_name",
  "request_id": "unique_request_id",
  "error_code": -2,
  "error_message": "Permission denied"
}
```

**Note:** Error codes must match the OFSErrorCodes enum values exactly.

### Required Functions to Implement

#### Core System Functions

| Function | Parameters | Returns | Description |
|----------|------------|---------|-------------|
| fs_init | void** instance, const char* omni_path, const char* config_path | int | Initialize file system, load all structures into memory |
| fs_shutdown | void* instance | void | Cleanup and shutdown file system |
| fs_format | const char* omni_path, const char* config_path | int | Create new .omni file with specified configuration |

**Data Structure Consideration for fs_init:**
- This function must load users, files, and free space information
- Think about which structures will be accessed frequently
- Consider the trade-off between loading speed and operation speed

#### User Management Functions

| Function | Parameters | Returns | Who Can Use |
|----------|------------|---------|-------------|
| user_login | void** session, const char* username, const char* password | int | Anyone - Create new user session |
| user_logout | void* session | int | Logged-in users - End session |
| user_create | void* admin_session, const char* username, const char* password, UserRole role | int | Admin only - Create new user account |
| user_delete | void* admin_session, const char* username | int | Admin only - Remove user account |
| user_list | void* admin_session, UserInfo** users, int* count | int | Admin only - Get list of all users |
| get_session_info | void* session, SessionInfo* info | int | Logged-in users - Get current session details |

**Data Structure Consideration:**
- user_login: Needs fast username lookup - think about optimal structure
- user_list: Needs to iterate all users and return array of UserInfo - structure must support this
- user_create/user_delete: Needs dynamic add/remove - structure must handle this efficiently
- Return Values: All functions return OFSErrorCodes values (0 for success, negative for errors)

#### File Operations

| Function | Parameters | Returns | Description |
|----------|------------|---------|-------------|
| file_create | void* session, const char* path, const char* data, size_t size | int | Create new file with initial data |
| file_read | void* session, const char* path, char** buffer, size_t* size | int | Read file content into allocated buffer |
| file_edit | void* session, const char* path, const char* data, size_t size, uint index | int | Writes at the given index of the file. |
| file_delete | void* session, const char* path | int | Delete specified file |
| file_truncate | void* session, const char* path | int | Remove the content of the file and write siruamr on the complete file. |
| file_exists | void* session, const char* path | int | Check if file exists (returns OFS_SUCCESS if exists) |
| file_rename | void* session, const char* old_path, const char* new_path | int | Rename/move file |

**Data Structure Consideration:**
- All file operations need fast path resolution (/dir1/dir2/file.txt → file location)
- Think about how to traverse directory paths efficiently
- Consider how to update indices when files are created/deleted/renamed

#### Directory Operations

| Function | Parameters | Returns | Description |
|----------|------------|---------|-------------|
| dir_create | void* session, const char* path | int | Create new directory |
| dir_list | void* session, const char* path, FileEntry** entries, int* count | int | Get list of files in directory |
| dir_delete | void* session, const char* path | int | Delete directory (must be empty) |
| dir_exists | void* session, const char* path | int | Check if directory exists |

**Data Structure Consideration:**
- dir_list: Needs to efficiently retrieve all children of a directory and return as FileEntry array
- dir_delete: Must verify directory is empty - how to check this quickly?
- Think about how to represent parent-child relationships in your structure

#### Information Functions

| Function | Parameters | Returns | Description |
|----------|------------|---------|-------------|
| get_metadata | void* session, const char* path, FileMetadata* meta | int | Get detailed file information |
| set_permissions | void* session, const char* path, uint32_t permissions | int | Change file permissions |
| get_stats | void* session, FSStats* stats | int | Get file system statistics |
| free_buffer | void* buffer | void | Free memory allocated by file_read |
| get_error_message | int error_code | const char* | Get descriptive error message |

**Data Structure Consideration:**
- get_stats: Needs to quickly compute total files, total size, free space - returns FSStats structure
- Think about whether to cache statistics or compute them on-demand
- Consider maintaining summary counters that update with each operation

### User Interface Requirements

You are free to create any type of UI using any programming language or framework. The only requirements are:
- Must communicate via sockets - Connect to your server on port 8080
- Cannot be simple terminal commands - Must have a graphical or web-based interface
- Must send/receive JSON messages - Follow the socket protocol
- Must use standard error codes - Interpret OFSErrorCodes values correctly

#### UI Portability
Because all implementations use the same:
- Error codes (OFSErrorCodes)
- User roles (UserRole)
- Structure definitions (UserInfo, FileEntry, etc.)
- Socket protocol (JSON format)

Your UI can work with ANY student's backend implementation! This means:
- You can test your backend with other students' UIs
- The teacher can use a standard testing UI across all submissions
- You can collaborate on UI development while focusing on backend

**UI Options (Examples):**
- Web Interface: HTML/CSS/JavaScript, React, Vue, Angular
- Desktop Application: Python (Tkinter/PyQt), Java (JavaFX), C# (WinForms/WPF)
- Mobile App: React Native, Flutter, Android/iOS native
- Terminal UI: Rich/Textual (Python), ncurses (C/C++) - Must have interactive UI, not just commands

### Documentation Requirements

You must document your implementation choices. Create these documents:

#### 1. Design Choices Document (design_choices.md)
Explain:
- Data structures chosen and WHY
- What structure did you use for user indexing and why?
- How do you represent the directory tree?
- What structure tracks free space?
- How do you map file paths to disk locations?
- How you structured the .omni file (header, data blocks, indexing)
- Memory management strategies
- Any optimizations you made

**Example of what we're looking for:**
```
"I chose a hash table for user indexing because user_login requires 
O(1) lookup by username. The hash table maps username → UserInfo 
structure. For collision handling, I used chaining with linked lists..."
```

#### 2. File I/O Strategy Document (file_io_strategy.md)
Describe:
- How you read from and write to the .omni file
- How you serialize/deserialize standard structures (OMNIHeader, UserInfo, FileEntry)
- Buffering strategies
- How you handle file growth
- How you manage free space
- Approach to data integrity
- What gets loaded into memory vs. read from disk per operation

#### 3. FIFO Queue Implementation (fifo_workflow.md)
Document:
- How your operation queue works
- Thread/process management approach
- How requests are queued and processed
- How responses are sent back to clients

#### 4. User Guide (user_guide.md)
Provide:
- How to compile and run your server
- How to use your UI
- Sample operations with screenshots
- Configuration options

#### 5. Testing Report (testing_report.md)
Include:
- Test scenarios you ran
- Performance metrics (operations per second, response times)
- Concurrent client testing results
- Edge cases tested
- Cross-compatibility testing (if you tested with other students' backends/UIs)

### Looking Ahead: Phase 2 - Delta Vault

Phase 2 will introduce tacking your file system. While you don't need to implement this now, start thinking about these concepts:

#### What is Delta Vault?
Delta Vault is a system integrated into your file system. Every time a file is modified, the system:
- Stores only the changes (deltas) - not entire file copies
- Maintains history - can view/restore any previous state
- Tracks who made changes - audit trail of all modifications
- Enables rollback - restore files to any previous state

#### Core Concepts to Consider Now
Even though Phase 2 is later, think about:

1. **Metadata Storage:**
   - Where in your .omni file will you store this information?
   - Notice the fields in the struct, these are reserved for Phase 2
   - How will you link states to files?
   - What data structure represents history properly?

2. **Delta Storage:**
   - How will you store only the differences between states?
   - How will you reconstruct a file from multiple deltas?
   - What's more efficient: forward deltas or backward deltas?

3. **Change Tracking:**
   - What information do you need to track per change?
   - Timestamp, user, file path, change type
   - How will you structure the change log?
   - Should it be in-memory, on-disk, or both?

4. **Retrieval:**
   - How will you quickly find all states of a file?
   - What structure allows efficient "get state N of file.txt"?
   - How do you handle chains (s1 → s2 → s3)?

#### Design Consideration for Phase 1
Smart students will design their Phase 1 system with Phase 2 in mind:
- The OMNIHeader already has reserved space for state control metadata
- Design your file storage to accommodate delta storage later
- Think about how your indexing structures might need to expand
- Consider how history affects your free space management

You don't need to implement this now, but thinking about it will make your Phase 1 design more flexible and extensible.

**Example forward-thinking:**
```
"In my file metadata structure, I included a linked list pointer that's 
currently NULL but will point to file history in Phase 2. My block 
allocation strategy reserves 20% of space for future delta storage by 
marking those blocks as 'reserved' in my free space bitmap."
```

### Submission Structure
Submit a ZIP file named `<figure_out>.zip` with this structure (hint: use PHASE-1):

```
studentid_phase1.zip/
├── source/
│   ├── server/              # Your socket server code
│   ├── core/                 # File system implementation
│   ├── data_structures/     # Your data structure implementations
│   ├── include/
│   │   └── ofs_types.h      # MUST include standard types header
│   └── ui/                  # Your UI source code
├── compiled/
│   ├── sample.omni          # Sample .omni file
│   ├── default.uconf        # Configuration file
├── documentation/
│   ├── design_choices.md
│   ├── file_io_strategy.md
│   ├── user_guide.md
│   └── testing_report.md
└── README.md                # Quick start guide
```

### Step 1: Design Your Data Structures FIRST
Before writing any code, answer these questions on paper:

**User Management:**
- What structure will hold users in memory?
- How will you lookup users by username?
- How will you iterate all users?

**File System Tree:**
- How will you represent directories and files?
- How will you traverse paths like /a/b/c/file.txt?
- How will you list all files in a directory?

**Free Space Tracking:**
- How will you know which blocks are free?
- How will you allocate N consecutive blocks?
- How will you handle fragmentation?

**File Location Mapping:**
- How will you map file paths to physical locations in .omni?
- What enables O(1) or O(log n) lookup?

Sketch your structures on paper, discuss with peers, think deeply before coding.

### Step 2: Implement Core File Operations
Start with basic operations:
- Create a new .omni file
- Write data to the file
- Read data from the file
- Update existing data

### Step 3: Build the Socket Server
Create a server that:
- Accepts connections
- Receives JSON messages
- Parses requests
- Calls your core functions

### Step 4: Implement FIFO Queue
Add queuing logic:
- Queue incoming requests
- Process one at a time
- Send responses back

### Step 5: Create Your UI
Build an interface that:
- Connects to your server
- Sends formatted requests
- Displays responses
- Provides good user experience

### Important Notes
- All data structures must be implemented by you - No using built-in file systems or databases
- Single .omni file - Everything must be in one binary file
- FIFO processing - Operations execute sequentially, one at a time
- Document your structure choices - Explain WHY you chose each structure
- Think about Phase 2 - Design with state control in mind
- Test thoroughly - Try multiple concurrent clients
- UI is mandatory - Cannot be just terminal commands

### Tips for Success
- Design before coding - Spend serious time thinking about structures
- Justify your choices - "I used X because Y" is what we want to see
- Test incrementally - Don't wait until everything is done to test
- Think about trade-offs - Memory vs. speed, simplicity vs. efficiency
- Consider Phase 2 - Design for extensibility
- Handle errors - Think about what can go wrong
- Ask questions - If requirements are unclear, ask for clarification

The data structure choices you make will determine your system's performance and elegance.

**Remember:** There's no single "correct" structure for each problem. What matters is that you:
- Understand the trade-offs
- Make informed decisions
- Document your reasoning
- Implement it correctly