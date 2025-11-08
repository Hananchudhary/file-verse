# OFS Project
## Core File System Design (Part of Phase 1)

The following defines how data is physically laid out and managed within the single .omni container file. You must implement these structures and management strategies entirely from scratch.

## 1. Physical Layout and Pointers

The entire .omni file is a contiguous block of data, logically divided into three main areas after the fixed-size OMNIHeader and User Table (which stores UserInfo structs):

- **Metadata Index Area**: Stores the fixed-size structure for every file and directory in the system.
- **Free Space Tracking Area**: A simple map or list that tracks whether each unit of storage is used or free.
- **Content Block Area**: Stores the actual file data and directory listings.

### Pointers and Addressing

Since there are no built-in file systems, all locations are tracked using fixed-size integer Indices, not byte offsets.

- **Block Index**: A number starting from 1 that refers to a fixed-size storage unit (e.g., 4096 bytes) in the Content Block Area.
- **Entry Index**: A number starting from 1 that refers to a fixed-size slot in the Metadata Index Area.

## 2. File/Directory Metadata Structure

Every file and directory in the system requires a fixed, identical amount of space in the Metadata Index Area. This structure must serve as the primary lookup for all file properties and its physical location.

The size of this metadata structure should be fixed (e.g., 72 bytes) and must contain the following essential fields, based on your requirements:

| Field Name | Data Type | Key Purpose | Notes |
|------------|-----------|-------------|-------|
| Validity Flag | uint8_t | 0 = In Use, 1 = Unused/Free. | INVALID (bit) |
| Type Flag | uint8_t | 0 = File, 1 = Directory. | DIR/FILE (bit) |
| Parent Index | uint32_t | The Entry Index of the parent directory. Root directory is 0. | PARENT_INDEX |
| Short Name | char[12] | The name (up to 10 characters plus null-terminator). | FILENAME (<=10 bytes) |
| Start Index | uint32_t | The Block Index where the file's content begins. 0 if empty/unused. | START_INDEX |
| Total Size | uint64_t | The logical size of the file content in bytes. | TOTAL_SIZE |
| Owner/Permissions | uint32_t[] | Fields to track the file's owner and permissions. | (From FileEntry) |
| Timestamps | uint64_t[] | Fields for creation and modification times. | (From FileEntry) |

### Root Directory
The entry at Index 1 of the Metadata Index Area must be permanently reserved for the Root Directory (/).

## 3. Content Block Allocation and Linking

The Content Block Area stores the actual data. It is a large array of uniform, fixed-size blocks (64K bytes).

### Block Structure
To handle files larger than a single block, you must implement a simple Linked List strategy directly within the data blocks.

Each block must reserve the first 4 bytes for a single purpose: storing the Block Index of the next block in the file's chain.

A value of 0 in this pointer means this is the last block of the file.

The remaining bytes of the block are dedicated to file content.

| Field Name | Size (Bytes) | Purpose |
|------------|--------------|---------|
| Next Block Pointer | 4 | Block Index of the next block in the file. 0 indicates the end of the file. |
| Content Area | BlockSize - 4 | The space used for the actual file data. |

**File Access:** To read a file, your system starts at the Start Index found in the metadata, reads the content, retrieves the Next Block Pointer, and repeats until the pointer is 0.

### Directory Content
Directory contents are not stored in their metadata entry. Instead, a directory's Start Index points to a block (or chain of blocks) that holds a list of the Entry Indices (the location in the Metadata Index Area) of all its children.

## 4. Free Space Management

Your system must efficiently track which blocks in the Content Block Area are available.

**Strategy:** Implement a simple and fast Map of Usage that resides in the Free Space Tracking Area (and is loaded into memory during fs_init).

**Mechanism:** This map must track, for every single block in the Content Block Area, whether it is Used or Free (a simple binary status).

**Operations:**
- **Allocation:** When a file needs $N$ blocks, the system must quickly consult this map to find $N$ free blocks, mark them as Used, and link them together using the Next Block Pointers.
- **Deallocation:** When a file is deleted, the system traverses its block chain and marks all those blocks as Free in the map.

## 5. Security Requirements

### File Content Encoding
To fulfill the encryption requirement, you must implement a simple Byte Substitution Encoding.

- **Mapping:** Design a custom 256-byte array or map that defines a one-to-one replacement: original_byte $\rightarrow$ encoded_byte.
- **Rule:** All file content read from or written to the Content Block Area must be passed through this encoding/decoding process. The mapping itself must be stored within the .omni file (e.g., in the reserved header space).

### Key-Based System Visibility
The backend must store a Private Key (e.g., 64 bytes) within the .omni file.

- **Check:** Every frontend request must pass a matching key as a parameter.
- **Stealth Policy:** If the received private_key DOES NOT MATCH the stored key, the backend must immediately abort the operation and should become silent. The system must appear to be empty or non-functional without the correct key.

---

## Hint:
Think about upfront loading for which you have to think about which data structures you should use.
- user-info, Loading file paths for better indexing (searching too), Finding free space.

Add log activity for each operation so that you can easily implement phase 2.

## Bonuses:
- Finding Free Space efficiently (focusing both on the space complexity too)
- Do not initialize data structures on each system start (you have to also save them in a way you can easily load later without computing again)
- Implement Caching
- All of this is based on a banking system but if it was a collaborative system like google sheets or something like that, handle that multiuser scenario.
- Any other commendable bonus you can find.