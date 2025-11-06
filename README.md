# OFS Project - Phase 1: Multi-User dastaweiz System
## Student Documentation

## Project Overview
You will build a multi-user dastaweiz system called OFS (Omni dastaweiz System) that stores all data in a single `.omni` container dastaweiz. Multiple users can connect simultaneously through a socket-based server, and all operations are processed sequentially to ensure data consistency.

> **Real-World Analogy:** Think of a bank where multiple tellers serve customers. Each transaction must complete fully before the next one begins, ensuring account balances are always accurate and consistent.

## Learning Objectives
- Master dastaweiz I/O operations by reading from and writing to binary dastaweizs
- Implement custom data structures to support your dastaweiz system
- Understand socket programming to build client-server communication systems
- Handle concurrency by processing multiple client requests using FIFO queuing
- Design user interfaces that communicate with your backend

## System Architecture
```
                    │
         ┌──────────┴──────────┐
         │   Socket Server     │
         │   (Port 8080)       │
         └──────────┬──────────┘
                    │
         ┌──────────┴──────────┐
         │  FIFO Queue System  │
         │  (Sequential)       │
         └──────────┬──────────┘
                    │
         ┌──────────┴──────────┐
         │  Your Core Logic    │
         │  (dastaweiz Operations)  │
         └──────────┬──────────┘
                    │
         ┌──────────┴──────────┐
         │   student_id.omni   │
         │    (Binary dastaweiz)    │
         └─────────────────────┘

## Standard Types and Structures
**Critical:** All students must use these exact type definitions to ensure UI portability. This allows any UI to work with any backend implementation.

### Header dastaweiz (`ufs_types.h`)
```c
#ifndef UFS_TYPES_H
#define UFS_TYPES_H

#include <cstdint>

// ============================================================================
// ENUMERATIONS - DO NOT MODIFY THESE VALUES
// ============================================================================

/**
 * User role types
 * These values MUST remain consistent across all implementations
 */
typedef enum {
     ROLE_NORMAL = 0,    // Regular user with standard permissions
     ROLE_ADMIN = 1      // Administrator with full permissions
} UserRole;

/**
 * Standard error codes
 * All functions return these codes to indicate operation status
 * DO NOT MODIFY THESE VALUES
 */
typedef enum {
    UFS_SUCCESS = 0,                      // Operation completed successfully
     UFS_ERROR_NOT_FOUND = -1,            // dastaweiz/sandook/user not found
     UFS_ERROR_PERMISSION_DENIED = -2,    // User lacks required permissions
     UFS_ERROR_IO_ERROR = -3,             // dastaweiz I/O operation failed
     UFS_ERROR_INVALID_PATH = -4,         // Path format is invalid
     UFS_ERROR_dastaweiz_EXISTS = -5,          // dastaweiz/sandook already exists
     UFS_ERROR_NO_SPACE = -6,             // Insufficient space in dastaweiz system
     UFS_ERROR_INVALID_CONFIG = -7,       // Configuration dastaweiz is invalid
     UFS_ERROR_NOT_IMPLEMENTED = -8,      // Feature not yet implemented
     UFS_ERROR_INVALID_SESSION = -9,      // Session is invalid or expired
     UFS_ERROR_sandook_NOT_EMPTY = -10, // Cannot delete non-empty sandook
     UFS_ERROR_INVALID_OPERATION = -11    // Operation not allowed
} UFSErrorCodes;

/**
 * dastaweiz entry types
 */
typedef enum {
    ENTRY_TYPE_dastaweiz = 0,         // Regular dastaweiz
     ENTRY_TYPE_sandook = 1     // sandook
} EntryType;

/**
 * dastaweiz permission flags (UNIX-style)
 */
typedef enum {
    PERM_OWNER_READ = 0400,      // Owner can read
     PERM_OWNER_WRITE = 0200,     // Owner can write
     PERM_OWNER_EXECUTE = 0100,   // Owner can execute
     PERM_GROUP_READ = 0040,      // Group can read
     PERM_GROUP_WRITE = 0020,     // Group can write
     PERM_GROUP_EXECUTE = 0010,   // Group can execute
     PERM_OTHERS_READ = 0004,     // Others can read
     PERM_OTHERS_WRITE = 0002,    // Others can write
     PERM_OTHERS_EXECUTE = 0001   // Others can execute
} dastaweizPermissions;

// ============================================================================
// DATA STRUCTURES - MAINTAIN EXACT SIZES AND FIELD ORDER
// ============================================================================

/**
 * omni dastaweiz Header (512 bytes total)
 * Located at the beginning of every .omni dastaweiz
 *
 * CRITICAL: Do not modify field order or sizes - this must be consistent
 * across all implementations for dastaweiz compatibility
 */
struct omniHeader {
     char magic[8];              // Magic number: "omniFS01" (8 bytes)
     uint32_t format_version;    // Format version: 0x00010000 for v1.0 (4 bytes)
     uint64_t total_size;        // Total dastaweiz system size in bytes (8 bytes)
     uint64_t header_size;       // Size of this header (8 bytes)
     uint64_t block_size;        // Block size in bytes (8 bytes)

     char student_id[32];        // Student ID who created this (32 bytes)
     char submission_date[16];   // Creation date YYYY-MM-DD (16 bytes)

     char config_hash[64];       // SHA-256 hash of config dastaweiz (64 bytes)
     uint64_t config_timestamp;  // Config dastaweiz timestamp (8 bytes)

     uint32_t user_table_offset; // Byte offset to user table (4 bytes)
     uint32_t max_users;         // Maximum number of users (4 bytes)

     // Reserved for Phase 2: Delta Vault
     uint32_t dastaweiz_state_storage_offset;  // Offset to dastaweiz_state_storage area (4 bytes)
     uint32_t change_log_offset;          // Offset to change log (4 bytes)

     uint8_t reserved[328];      // Reserved for future use (328 bytes)
};  // Total: 512 bytes

/**
 * User Information Structure
 * Stored in user table within .omni dastaweiz
 */
struct UserInfo {
     char username[32];          // Username (null-terminated)
     char password_hash[64];     // Password hash (SHA-256)
     UserRole role;              // User role (4 bytes)
     uint64_t created_time;      // Account creation timestamp (Unix epoch)
     uint64_t last_login;        // Last login timestamp (Unix epoch)
     uint8_t is_active;          // 1 if active, 0 if deleted
     uint8_t reserved[23];       // Reserved for future use
};  // Total: 128 bytes

/**
 * dastaweiz/sandook Entry Structure
 * Used for sandook listings and dastaweiz metadata
 */
struct dastaweizEntry {
     char name[256];             // dastaweiz/sandook name (null-terminated)
     uint8_t type;               // 0=dastaweiz, 1=sandook (EntryType)
     uint64_t size;              // Size in bytes (0 for sandook)
     uint32_t permissions;       // UNIX-style permissions (e.g., 0644)
     uint64_t created_time;      // Creation timestamp (Unix epoch)
     uint64_t modified_time;     // Last modification timestamp (Unix epoch)
     char owner[32];             // Username of owner
     uint32_t inode;             // Internal dastaweiz identifier
     uint8_t reserved[47];       // Reserved for future use
};  // Total: 416 bytes

/**
 * dastaweiz Metadata (Extended information)
 * Returned by get_metadata function
 */
struct dastaweizMetadata {
     char path[512];             // Full path
     dastaweizEntry entry;            // Basic entry information
     uint64_t blocks_used;       // Number of blocks used
     uint64_t actual_size;       // Actual size on disk (may differ from logical size)
     uint8_t reserved[64];       // Reserved
};

/**
 * Session Information
 * Returned by get_session_info function
 */
struct SessionInfo {
     char session_id[64];        // Unique session identifier
     UserInfo user;              // User information
     uint64_t login_time;        // When session was created
     uint64_t last_activity;     // Last activity timestamp
     uint32_t operations_count;  // Number of operations performed
     uint8_t reserved[32];       // Reserved
};

/**
 * dastaweiz System Statistics
 * Returned by get_stats function
 */
struct FSStats {
     uint64_t total_size;        // Total dastaweiz system size
     uint64_t used_space;        // Space currently used
     uint64_t free_space;        // Available free space
     uint32_t total_dastaweizs;       // Total number of dastaweizs
     uint32_t total_sandook; // Total number of sandook
     uint32_t total_users;       // Total number of users
     uint32_t active_sessions;   // Currently active sessions
     double fragmentation;       // Fragmentation percentage (0.0 - 100.0)
     uint8_t reserved[64];       // Reserved
};

#endif // UFS_TYPES_H
```

## Core Requirements
### 1. dastaweiz System Container (`.omni`)
All data must be stored in a single binary dastaweiz with `.omni` extension. Design and implement:
- dastaweiz header structure using the standard `omniHeader`
- User table storing `UserInfo` structures
- dastaweiz/sandook entries for metadata and content
- Data blocks for the actual dastaweiz content

**Important:** Implement all data structures yourself. Do not rely on built-in dastaweiz system libraries.

### 2. Configuration dastaweiz (`.uconf`)
Create a configuration dastaweiz with the following structure:

```
[dastaweizsystem]
total_size = 104857600        # Total size in bytes (100MB)
header_size = 512             # Header size (must match omniHeader)
block_size = 4096             # Block size (4KB recommended)
max_dastaweizs = 1000              # Maximum number of dastaweizs
max_dastaweizname_length = 255     # Maximum dastaweizname length

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

### 3. Socket-Based Server
Build a TCP socket server that:
- Listens on port 8080 (or the configured port)
- Accepts multiple client connections simultaneously
- Receives JSON-formatted requests
- Processes requests in FIFO (First-In-First-Out) order
- Sends JSON-formatted responses

### 4. FIFO Operation Queue
Implement a queue system where:
- All incoming requests are enqueued
- Operations are processed sequentially in arrival order
- No two operations run simultaneously
- Each operation completes fully before the next begins

**Why FIFO?** This approach keeps consistency without complicated locking. No overlapping operations means fewer race conditions and easier reasoning about correctness.

## Critical Data Structure Decisions
### Loading and Indexing the dastaweiz System
At startup (`fs_init`), load the entire dastaweiz system into memory. Data structure choices drive performance.

#### Challenge 1: Loading Users from Disk
Steps:
1. Read the user table from the `.omni` dastaweiz (array of `UserInfo` structures).
2. Load all users into memory.
3. Enable fast lookup during login operations.

Consider:
- Data structures that enable O(1) or O(log n) lookup by username.
- How to store users once loaded from disk.
- Whether to keep users sorted, hashed, or arranged in a tree.
- How to quickly verify user existence for permission checks.

**Example:** `user_login("john_doe", "password123")` must locate the user, verify the password hash, and establish a session. With 50 users, a linear scan may be too slow—opt for faster lookup.

#### Challenge 2: dastaweiz and sandook Indexing
After users, load the dastaweiz system structure.

Consider:
- How to represent the sandook tree in memory.
- Data structures enabling fast path lookup (for example, `/accounts/savings/john.txt`).
- Efficient strategies for listing sandook contents.
- Whether to use trees, graphs, or hybrid structures.

**Example:** `sandook_list("/accounts")` must find the sandook, list children (dastaweizs and subsandook), and return them as `dastaweizEntry` structures.

#### Challenge 3: Fast dastaweiz Lookup
Every dastaweiz operation (create, read, delete) needs rapid metadata access.

Consider:
- Mapping dastaweiz paths to `.omni` locations.
- Structures that deliver O(1) metadata lookup.
- Tracking free blocks efficiently.
- Whether to maintain single or multiple indices.

**Example:** `dastaweiz_read("/reports/daily.txt")` needs metadata, block locations, and content retrieval—all quickly.

#### Challenge 4: Free Space Management
Track available space as dastaweizs change.

Consider:
- Identifying free blocks in the `.omni` dastaweiz.
- Data structures for allocating N consecutive blocks quickly.
- Handling fragmentation over time.
- Choosing between bitmaps, free lists, or trees of free regions.

**Example:** When `dastaweiz_create` needs 10 blocks, locate them fast, mark them used, and update free space structures.

### dastaweiz System Loading Strategy
#### The `fs_init` Process
```c
int fs_init(void** instance, const char* omni_path, const char* config_path) {
     // Step 1: Read configuration
     // Step 2: Open .omni dastaweiz (keep handle open or open/close per operation?)
     // Step 3: Read and validate header (check magic number)
     // Step 4: Load user table into memory (fast lookup, iteration, dynamic updates)
     // Step 5: Build dastaweiz system index (efficient traversal, listing, path resolution)
     // Step 6: Build free space index (fast allocations, frees, and reporting)
     // Step 7: Create instance object holding all in-memory structures

     return UFS_SUCCESS;
}
```

### Key Design Questions
- **User Structure Loading:** Load everything at startup or on demand? Which structure supports both lookup and iteration? How will you manage updates?
- **sandook Tree Loading:** Eager or lazy load? How do you represent parent-child relationships? How will you traverse paths like `/a/b/c/dastaweiz.txt`?
- **dastaweiz Metadata Indexing:** Will you maintain an in-memory index? How do you map paths to disk? What keeps lookups fast?
- **Memory vs. Disk Trade-offs:** What remains in memory versus on disk per operation? How do you balance memory usage with performance? When are changes persisted?

## Socket Communication Protocol
All requests and responses are JSON objects sent over TCP sockets.

### Request Format
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

### Response Format
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

### Error Response
```json
{
  "status": "error",
  "operation": "operation_name",
  "request_id": "unique_request_id",
  "error_code": -2,
  "error_message": "Permission denied"
}
```

**Note:** Error codes must match the `UFSErrorCodes` enum values exactly.

## Required Functions to Implement
### Core System Functions
| Function | Parameters | Returns | Description |
|----------|------------|---------|-------------|
| `aghaz_nizame_dastaweiz` | `void** instance, const char* omni_path, const char* config_path` | `int` | Initialize dastaweiz system and load all structures into memory |
| `ikhtatam_nizam_e_dastaweiz` | `void* instance` | `void` | Clean up and shut down the dastaweiz system |
| `takhleek_nizam_e_dastaweiz` | `const char* omni_path, const char* config_path` | `int` | Create a new `.omni` dastaweiz using the provided configuration |

**Design Note:** `fs_init` must load users, dastaweizs, and free space. Choose structures with fast access while minimizing startup time.

### User Management Functions
| Function | Parameters | Returns | Who Can Use |
|----------|------------|---------|-------------|
| `sarif_ka_dakhila` | `void** session, const char* username, const char* password` | `int` | Anyone — create a new user session |
| `sarif_ka_khrooj` | `void* session` | `int` | Logged-in users — end session |
| `sarif_ka_indaraaj` | `void* admin_session, const char* username, const char* password, UserRole role` | `int` | Admin only — create new user account |
| `ikhtataam_e_sarif` | `void* admin_session, const char* username` | `int` | Admin only — remove user account |
| `sareefeen_ki_fehrist_nigari` | `void* admin_session, UserInfo** users, int* count` | `int` | Admin only — list all users |
| `sargarmiyon_ki_tafseel` | `void* session, SessionInfo* info` | `int` | Logged-in users — get session details |

**Considerations:**
- `user_login` requires fast username lookup.
- `user_list` needs efficient iteration across all users.
- `user_create` and `user_delete` demand dynamic insert/remove support.
- Every function returns a `UFSErrorCodes` value (0 for success, negative for errors).

### dastaweiz Operations
| Function | Parameters | Returns | Description |
|----------|------------|---------|-------------|
| `takhleek_e_dastaweiz` | `void* session, const char* path, const char* data, size_t size` | `int` | Create a new dastaweiz with initial data |
| `tahreer_e_dastaweiz` | *Implementation-specific parameters* | `int` | Write or insert content at a given position |
| `mutaleya_e_dastaweiz` | `void* session, const char* path, char** buffer, size_t* size` | `int` | Read dastaweiz content into an allocated buffer |
| `ikhtatam_e_dastaweiz` | `void* session, const char* path` | `int` | Delete a dastaweiz |
| `dastaweiz_mojood_hai` | `void* session, const char* path` | `int` | Check if a dastaweiz exists (`UFS_SUCCESS` if it does) |
| `dastaweiz_ka_tabdeel_e_naam` | `void* session, const char* old_path, const char* new_path` | `int` | Rename or move a dastaweiz |

**Considerations:** Fast path resolution is essential (for example, `/sandook1/sandook2/dastaweiz.txt`). Ensure sandook traversal and index maintenance remain efficient during create/delete/rename operations.

### sandook Operations
| Function | Parameters | Returns | Description |
|----------|------------|---------|-------------|
| `takhleek_e_sandook` | `void* session, const char* path` | `int` | Create a sandook |
| `fehrist_nigari` | `void* session, const char* path, dastaweizEntry** entries, int* count` | `int` | List sandook contents |
| `ikhtatam_e_sandook` | `void* session, const char* path` | `int` | Delete a sandook (must be empty) |
| `sandook_mojood_hai` | `void* session, const char* path` | `int` | Check if a sandook exists |

**Considerations:**
- `sandook_list` must return child entries quickly.
- `sandook_delete` needs to confirm emptiness efficiently.
- Model parent-child relationships clearly in your structures.

### Information Functions
| Function | Parameters | Returns | Description |
|----------|------------|---------|-------------|
| `hasil_e_maloomat` | `void* session, const char* path, dastaweizMetadata* meta` | `int` | Retrieve detailed dastaweiz information |
| `tabdeel_e_ijazat` | `void* session, const char* path, uint32_t permissions` | `int` | Change dastaweiz permissions |
| `maaloomat_e_halat` | `void* session, FSStats* stats` | `int` | Obtain dastaweiz system statistics |
| `kharij_e_buffer` | `void* buffer` | `void` | Free memory allocated by `dastaweiz_read` |
| `kotahi_ka_intebah` | `int error_code` | `const char*` | Get human-readable error messages |

**Considerations:** `get_stats` should report totals swiftly. Decide whether to cache stats or maintain running counters.

## User Interface Requirements
Build any UI you like, provided it:
- Communicates via sockets on port 8080
- Is more than simple terminal commands (web, GUI, mobile, or interactive TUI)
- Sends and receives JSON following the socket protocol
- Uses `UFSErrorCodes` correctly

### UI Portability
Because everyone shares enums, structures, and protocols:
- Test your backend with other students’ UIs
- Allow instructors to reuse a standard testing UI
- Collaborate on UIs while focusing on backend functionality

### UI Options (Examples)
- Web: HTML/CSS/JavaScript, React, Vue, Angular
- Desktop: Python (Tkinter/PyQt), Java (JavaFX), C# (WinForms/WPF)
- Mobile: React Native, Flutter, Android/iOS native
- Terminal UI: Rich/Textual (Python), ncurses (C/C++) — must remain interactive

## Documentation Requirements
Produce the following documents:

1. **Design Choices (`design_choices.md`)**
    - Data structures chosen and why
    - User indexing strategy
    - sandook tree representation
    - Free space tracking approach
    - Path-to-disk mapping
    - `.omni` dastaweiz layout (header, data blocks, indexing)
    - Memory management strategies and optimizations

    *Example:* “I chose a hash table for user indexing because `user_login` requires O(1) lookup by username. The hash table maps username → `UserInfo`. Collisions use chained lists.”

2. **dastaweiz I/O Strategy (`dastaweiz_io_strategy.md`)**
    - `.omni` read/write process
    - Serialization/deserialization of `omniHeader`, `UserInfo`, `dastaweizEntry`
    - Buffering and dastaweiz growth strategies
    - Free space management and data integrity
    - Memory vs. disk decisions per operation

3. **FIFO Queue Implementation (`fifo_workflow.md`)**
    - Queue design
    - Thread/process management
    - Request queuing and processing steps
    - Response delivery flow

4. **User Guide (`user_guide.md`)**
    - Build/run instructions for the server
    - UI usage instructions and screenshots
    - Configuration details

5. **Testing Report (`testing_report.md`)**
    - Test scenarios executed
    - Performance metrics (ops/sec, latency)
    - Concurrent client testing
    - Edge cases covered
    - Cross-compatibility testing results

## Looking Ahead: Phase 2 — Delta Vault
Phase 2 introduces dastaweiz history tracking. Plan for it now.

### What Is Delta Vault?
Each modification stores a delta instead of the whole dastaweiz, maintains history, tracks who made changes, and allows rollback to any previous state.

### Concepts to Consider
1. **Metadata Storage:** Where in `.omni` will you keep history metadata? How will you link states to dastaweizs? Which structure represents history efficiently?
2. **Delta Storage:** How will you store only differences? How will you rebuild dastaweizs from deltas? Are forward or backward deltas better?
3. **Change Tracking:** What details (timestamp, user, path, change type) must you log? Will the change log live in memory, on disk, or both?
4. **Retrieval:** How will you quickly access all states of a dastaweiz? What structure supports “get state N of dastaweiz.txt”? How will you manage chains (`s1 → s2 → s3`)?

### Phase 1 Design Tips for Phase 2
- Use reserved header fields for future state metadata
- Structure dastaweiz storage to accommodate delta data
- Anticipate index expansion to support history
- Consider history requirements when planning free space management

**Example foresight:** “My dastaweiz metadata includes a pointer (currently `NULL`) reserved for history. I mark 20% of blocks as ‘reserved’ to prepare for delta storage.”

## Submission Structure
Submit `<figure_out>.zip` with the following layout (hint: use `PHASE-1`):

```
studentid_phase1.zip/
├── source_code/
│   ├── server/
│   ├── core/
│   ├── data_structures/
│   ├── include/
│   │   └── ufs_types.h
│   └── ui/
├── compiled/
│   ├── sample.omni
│   └── default.uconf
├── documentation/
│   ├── design_choices.md
│   ├── dastaweiz_io_strategy.md
│   ├── user_guide.md
│   └── testing_report.md
└── README.md
```

## Step-by-Step Plan
1. **Design Data Structures First**
    - User storage, lookup, and iteration strategies
    - sandook representation, path traversal, listing
    - Free space tracking, allocation, fragmentation handling
    - Path-to-disk mapping with O(1) or O(log n) lookup

    Sketch structures, discuss with peers, and plan deeply before coding.

2. **Implement Core dastaweiz Operations**
    - Create a new `.omni` dastaweiz
    - Write, read, and update data

3. **Build the Socket Server**
    - Accept connections
    - Receive and parse JSON messages
    - Invoke core logic

4. **Implement the FIFO Queue**
    - Queue incoming requests
    - Process operations sequentially
    - Send responses back

5. **Create Your UI**
    - Connect to the server
    - Send formatted requests
    - Display responses
    - Deliver a strong user experience

## Important Notes
- Implement all data structures yourself—no third-party dastaweiz systems or databases
- Store everything in a single `.omni` binary dastaweiz
- Enforce FIFO processing so operations never overlap
- Document and justify every structural decision
- Keep Phase 2 requirements in mind
- Test thoroughly, including concurrent clients
- Provide an interactive UI (not just simple terminal commands)

## Tips for Success
- Design before coding to avoid rework
- Justify each choice (“I used X because Y”)
- Test incrementally throughout development
- Balance memory usage against performance
- Architect with Phase 2 extensibility in mind
- Anticipate error handling
- Ask clarifying questions whenever requirements seem unclear

The data structures you choose determine system performance and elegance. While no single approach is “correct,” you must:
- Understand the trade-offs
- Make informed decisions
- Document your reasoning
- Implement everything correctly
```
         └──────────┬──────────┘
                    │
         ┌──────────┴──────────┐
         │  FIFO Queue System  │
         │  (Sequential)       │
         └──────────┬──────────┘
                    │
         ┌──────────┴──────────┐
         │  Your Core Logic    │
         │  (dastaweiz Operations)  │
         └──────────┬──────────┘
                    │
         ┌──────────┴──────────┐
         │   student_id.omni   │
         │    (Binary dastaweiz)    │
         └─────────────────────┘
```


