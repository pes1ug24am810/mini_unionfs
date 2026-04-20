# Mini-UnionFS: A FUSE-Based Layered Filesystem

## 🚀 Introduction
Mini-UnionFS is a simplified implementation of a **Union File System (UnionFS)** built using **FUSE (Filesystem in Userspace)**. It allows multiple directories, referred to as 'layers', to be overlaid into a single, unified merged view. 

This project demonstrates core operating system concepts such as layered storage, copy-on-write (COW) mechanisms, and virtualized file management without modifying the Linux kernel.

## ✨ Key Features
- **Layered Architecture**: Combines a read-only lower layer with a writable upper layer into a single mount point.
- **Copy-on-Write (COW)**: Ensures the integrity of the base data by copying files from the lower layer to the upper layer only when a modification is required.
- **Whiteout Mechanism**: Handles file deletions by creating special whiteout files (`.wh.filename`) in the upper layer to hide files existing in the lower layer.
- **Unified View**: Merges directory contents from both layers, ensuring that modifications in the upper layer take priority.

## 🛠 Supported File Operations
The system implements essential POSIX file operations through FUSE:
- `getattr`: Retrieves file metadata and permissions.
- `readdir`: Merges and lists directory entries from both layers.
- `read`: Accesses data from the resolved path in either layer.
- `write`: Handles data writing with integrated COW logic.
- `create` & `unlink`: Manages file creation and deletion.
- `mkdir` & `rmdir`: Manages directory lifecycle within the union structure.

## 🏗 System Architecture
1. **Lower Layer (Read-only)**: The base layer containing static files.
2. **Upper Layer (Writable)**: The layer where all new data, modifications, and whiteouts are stored.
3. **Mount Point**: The user-facing directory where the layers are merged.

**Path Resolution Logic:**
- Check if the file exists in the **Upper Layer**.
- If not found, check the **Lower Layer**.
- If neither exists (or a whiteout exists), return an error.

## 💻 Implementation Details

### Whiteout Handling
When a user deletes a file that exists in the lower layer, the system cannot remove it from the read-only source. Instead, it creates a file named `.wh.<filename>` in the upper layer. The `readdir` and `getattr` functions are designed to ignore any files masked by these whiteouts.

### Copy-on-Write (COW)
When a `write` operation is called on a file that currently only exists in the lower layer, the system:
1. Copies the entire file from the lower layer to the upper layer.
2. Applies the requested changes to the new copy in the upper layer.
3. Future reads and writes target this upper-layer version.

## 🧪 Testing
The project includes an automated test suite (`test_unionfs.sh`) that validates:
- **Layer Visibility**: Ensuring files from both layers appear in the merged view.
- **COW Functionality**: Confirming modifications stay in the upper layer.
- **Whiteout Mechanism**: Verifying that deleted lower-layer files are correctly hidden.

## 📜 Conclusion
This project provides a hands-on look at filesystem design and user-space kernel interaction. It successfully demonstrates how modern containerization technologies (like Docker) use layered filesystems to manage images and containers efficiently. 
 this is the code bro