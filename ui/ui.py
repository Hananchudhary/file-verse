import streamlit as st
import socket
import json
import time
import random
from datetime import datetime
from pathlib import Path

# Configuration
SERVER_HOST = "127.0.0.1"
SERVER_PORT = 8080

# Session state initialization
if 'logged_in' not in st.session_state:
    st.session_state.logged_in = False
if 'session_id' not in st.session_state:
    st.session_state.session_id = ""
if 'username' not in st.session_state:
    st.session_state.username = ""
if 'current_path' not in st.session_state:
    st.session_state.current_path = "/"
if 'file_contents' not in st.session_state:
    st.session_state.file_contents = {}

def generate_request_id():
    """Generate unique request ID"""
    return f"REQ_{int(time.time())}_{random.randint(0, 999999)}"

def send_request(operation, parameters=None, session_id=None):
    """Send JSON request to server and receive response"""
    if parameters is None:
        parameters = {}
    
    if session_id is None:
        session_id = st.session_state.session_id
    
    request = {
        "operation": operation,
        "session_id": session_id,
        "parameters": parameters,
        "request_id": generate_request_id()
    }
    
    try:
        # Create socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)  # 10 second timeout
        
        # Connect to server
        sock.connect((SERVER_HOST, SERVER_PORT))
        
        # Send request
        request_json = json.dumps(request)
        sock.sendall(request_json.encode())
        
        # Receive response
        response_data = b""
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response_data += chunk
            try:
                # Try to parse - if successful, we have complete JSON
                json.loads(response_data.decode())
                break
            except json.JSONDecodeError:
                continue
        
        sock.close()
        
        if response_data:
            response = json.loads(response_data.decode())
            return response
        else:
            return {"status": "error", "error_message": "No response from server"}
            
    except socket.timeout:
        return {"status": "error", "error_message": "Connection timeout"}
    except ConnectionRefusedError:
        return {"status": "error", "error_message": "Cannot connect to server. Is it running?"}
    except Exception as e:
        return {"status": "error", "error_message": f"Error: {str(e)}"}

def login_page():
    """Display login page"""
    st.title("🗂️ OFS - Omni File System")
    st.markdown("### Login to access your files")
    
    col1, col2, col3 = st.columns([1, 2, 1])
    
    with col2:
        with st.form("login_form"):
            username = st.text_input("Username", placeholder="Enter your username")
            password = st.text_input("Password", type="password", placeholder="Enter your password")
            submit = st.form_submit_button("Login", use_container_width=True)
            
            if submit:
                if not username or not password:
                    st.error("Please enter both username and password")
                else:
                    with st.spinner("Authenticating..."):
                        response = send_request("login", {
                            "username": username,
                            "password": password
                        }, session_id="")
                        
                        if response.get("status") == "success":
                            st.session_state.logged_in = True
                            st.session_state.session_id = response.get("session_id", "")
                            st.session_state.username = username
                            st.success("Login successful!")
                            time.sleep(0.5)
                            st.rerun()
                        else:
                            st.error(f"Login failed: {response.get('error_message', 'Unknown error')}")

def get_directory_contents(path):
    """Get contents of a directory"""
    response = send_request("list_directory", {"path": path})
    
    if response.get("status") == "success":
        entries = response.get("data", {}).get("entries", [])
        return entries
    return []

def display_breadcrumb():
    """Display current path as breadcrumb navigation"""
    parts = [p for p in st.session_state.current_path.split('/') if p]
    
    # Root button
    col_items = st.columns(len(parts) + 2)
    with col_items[0]:
        if st.button("🏠 Root", key="nav_root"):
            st.session_state.current_path = "/"
            st.rerun()
    
    # Path parts
    current_build_path = ""
    for i, part in enumerate(parts):
        current_build_path += "/" + part
        with col_items[i + 1]:
            if st.button(f"📁 {part}", key=f"nav_{i}_{part}"):
                st.session_state.current_path = current_build_path
                st.rerun()

def file_browser():
    """Display file browser interface"""
    st.markdown("### 📂 File Browser")
    
    # Breadcrumb navigation
    display_breadcrumb()
    st.markdown(f"**Current Path:** `{st.session_state.current_path}`")
    
    # Action buttons
    col1, col2, col3, col4 = st.columns(4)
    with col1:
        if st.button("➕ New File", use_container_width=True):
            st.session_state.show_create_file = True
    with col2:
        if st.button("📁 New Folder", use_container_width=True):
            st.session_state.show_create_dir = True
    with col3:
        if st.button("🔄 Refresh", use_container_width=True):
            st.rerun()
    with col4:
        if st.button("⬆️ Parent", use_container_width=True, 
                     disabled=(st.session_state.current_path == "/")):
            parent = str(Path(st.session_state.current_path).parent)
            st.session_state.current_path = parent if parent != "." else "/"
            st.rerun()
    
    st.markdown("---")
    
    # Create file dialog
    if st.session_state.get('show_create_file', False):
        with st.expander("Create New File", expanded=True):
            filename = st.text_input("File name:")
            content = st.text_area("File content:", height=150)
            
            col1, col2 = st.columns(2)
            with col1:
                if st.button("Create", use_container_width=True):
                    if filename:
                        file_path = f"{st.session_state.current_path}/{filename}".replace("//", "/")
                        response = send_request("create_file", {
                            "path": file_path,
                            "data": content
                        })
                        
                        if response.get("status") == "success":
                            st.success(f"File '{filename}' created!")
                            st.session_state.show_create_file = False
                            time.sleep(0.5)
                            st.rerun()
                        else:
                            st.error(response.get("error_message", "Failed to create file"))
                    else:
                        st.warning("Please enter a filename")
            
            with col2:
                if st.button("Cancel", use_container_width=True):
                    st.session_state.show_create_file = False
                    st.rerun()
    
    # Create directory dialog
    if st.session_state.get('show_create_dir', False):
        with st.expander("Create New Directory", expanded=True):
            dirname = st.text_input("Directory name:")
            
            col1, col2 = st.columns(2)
            with col1:
                if st.button("Create", key="create_dir_btn", use_container_width=True):
                    if dirname:
                        dir_path = f"{st.session_state.current_path}/{dirname}".replace("//", "/")
                        response = send_request("create_directory", {"path": dir_path})
                        
                        if response.get("status") == "success":
                            st.success(f"Directory '{dirname}' created!")
                            st.session_state.show_create_dir = False
                            time.sleep(0.5)
                            st.rerun()
                        else:
                            st.error(response.get("error_message", "Failed to create directory"))
                    else:
                        st.warning("Please enter a directory name")
            
            with col2:
                if st.button("Cancel", key="cancel_dir_btn", use_container_width=True):
                    st.session_state.show_create_dir = False
                    st.rerun()
    
    # Get directory contents
    entries = get_directory_contents(st.session_state.current_path)
    
    if not entries:
        st.info("📭 This directory is empty")
        return
    
    # Separate directories and files
    directories = [e for e in entries if e.get('type') == 1]
    files = [e for e in entries if e.get('type') == 0]
    
    # Display directories
    if directories:
        st.markdown("#### 📁 Directories")
        for entry in directories:
            display_directory_entry(entry)
    
    # Display files
    if files:
        st.markdown("#### 📄 Files")
        for entry in files:
            display_file_entry(entry)

def display_directory_entry(entry):
    """Display a directory entry"""
    name = entry.get('name', 'Unknown')
    
    col1, col2, col3 = st.columns([3, 1, 1])
    
    with col1:
        if st.button(f"📁 {name}", key=f"dir_{name}", use_container_width=True):
            new_path = f"{st.session_state.current_path}/{name}".replace("//", "/")
            st.session_state.current_path = new_path
            st.rerun()
    
    with col2:
        if st.button("ℹ️", key=f"info_dir_{name}"):
            st.session_state[f"show_info_{name}"] = True
            st.rerun()
    
    with col3:
        if st.button("🗑️", key=f"del_dir_{name}"):
            dir_path = f"{st.session_state.current_path}/{name}".replace("//", "/")
            response = send_request("delete_directory", {"path": dir_path})
            
            if response.get("status") == "success":
                st.success(f"Deleted '{name}'")
                time.sleep(0.5)
                st.rerun()
            else:
                st.error(response.get("error_message", "Failed to delete"))
    
    # Show info if requested
    if st.session_state.get(f"show_info_{name}", False):
        with st.expander(f"Info: {name}", expanded=True):
            st.write(f"**Name:** {name}")
            st.write(f"**Type:** Directory")
            st.write(f"**Owner:** {entry.get('owner', 'N/A')}")
            st.write(f"**Permissions:** {oct(entry.get('permissions', 0))}")
            if st.button("Close", key=f"close_info_{name}"):
                st.session_state[f"show_info_{name}"] = False
                st.rerun()

def display_file_entry(entry):
    """Display a file entry"""
    name = entry.get('name', 'Unknown')
    size = entry.get('size', 0)
    
    col1, col2, col3, col4, col5 = st.columns([2, 1, 1, 1, 1])
    
    with col1:
        st.markdown(f"📄 **{name}** ({size} bytes)")
    
    with col2:
        if st.button("👁️", key=f"view_{name}"):
            st.session_state[f"show_view_{name}"] = True
            st.rerun()
    
    with col3:
        if st.button("✏️", key=f"edit_{name}"):
            st.session_state[f"show_edit_{name}"] = True
            st.rerun()
    
    with col4:
        if st.button("ℹ️", key=f"info_{name}"):
            st.session_state[f"show_info_{name}"] = True
            st.rerun()
    
    with col5:
        if st.button("🗑️", key=f"del_{name}"):
            file_path = f"{st.session_state.current_path}/{name}".replace("//", "/")
            response = send_request("delete_file", {"path": file_path})
            
            if response.get("status") == "success":
                st.success(f"Deleted '{name}'")
                time.sleep(0.5)
                st.rerun()
            else:
                st.error(response.get("error_message", "Failed to delete"))
    
    # View file content
    if st.session_state.get(f"show_view_{name}", False):
        with st.expander(f"View: {name}", expanded=True):
            file_path = f"{st.session_state.current_path}/{name}".replace("//", "/")
            response = send_request("read_file", {"path": file_path})
            
            if response.get("status") == "success":
                content = response.get("data", {}).get("result_data", "")
                st.code(content, language="text")
            else:
                st.error(response.get("error_message", "Failed to read file"))
            
            if st.button("Close", key=f"close_view_{name}"):
                st.session_state[f"show_view_{name}"] = False
                st.rerun()
    
    # Edit file content
    if st.session_state.get(f"show_edit_{name}", False):
        with st.expander(f"Edit: {name}", expanded=True):
            file_path = f"{st.session_state.current_path}/{name}".replace("//", "/")

            # Read current content
            response = send_request("read_file", {"path": file_path})
            current_content = ""
            if response.get("status") == "success":
                current_content = response.get("data", {}).get("result_data", "")

            new_content = st.text_area(
                "Content:", 
                value=current_content, 
                height=200, 
                key=f"edit_area_{name}"
            )

            col1, col2 = st.columns(2)
            with col1:
                if st.button("Save", key=f"save_{name}", use_container_width=True):
                    # ⚠️ Use write_file instead of delete + create
                    write_resp = send_request("edit_file", {
                        "path": file_path,
                        "data": new_content,
                        "index" : 0
                    })

                    if write_resp.get("status") == "success":
                        st.success("File updated successfully!")
                        st.session_state[f"show_edit_{name}"] = False
                        time.sleep(0.5)
                        st.rerun()
                    else:
                        st.error(write_resp.get("error_message", "Failed to update file"))

        with col2:
            if st.button("Cancel", key=f"cancel_edit_{name}", use_container_width=True):
                st.session_state[f"show_edit_{name}"] = False
                st.rerun()

    
    # Show file info
    if st.session_state.get(f"show_info_{name}", False):
        with st.expander(f"Info: {name}", expanded=True):
            st.write(f"**Name:** {name}")
            st.write(f"**Type:** File")
            st.write(f"**Size:** {size} bytes")
            st.write(f"**Owner:** {entry.get('owner', 'N/A')}")
            st.write(f"**Permissions:** {oct(entry.get('permissions', 0))}")
            
            if st.button("Close", key=f"close_info_file_{name}"):
                st.session_state[f"show_info_{name}"] = False
                st.rerun()

def user_management():
    """User management interface (Admin only)"""
    st.markdown("### 👥 User Management")
    
    tab1, tab2, tab3 = st.tabs(["Create User", "Delete User", "List Users"])
    
    with tab1:
        st.markdown("#### Create New User")
        with st.form("create_user_form"):
            new_username = st.text_input("Username:")
            new_password = st.text_input("Password:", type="password")
            role = st.selectbox("Role:", ["Normal (0)", "Admin (1)"])
            
            if st.form_submit_button("Create User", use_container_width=True):
                role_value = "1" if "Admin" in role else "0"
                response = send_request("create_user", {
                    "username": new_username,
                    "password": new_password,
                    "role": role_value
                })
                
                if response.get("status") == "success":
                    st.success(f"User '{new_username}' created successfully!")
                else:
                    st.error(response.get("error_message", "Failed to create user"))
    
    with tab2:
        st.markdown("#### Delete User")
        with st.form("delete_user_form"):
            del_username = st.text_input("Username to delete:")
            
            if st.form_submit_button("Delete User", use_container_width=True):
                if del_username:
                    response = send_request("delete_user", {"username": del_username})
                    
                    if response.get("status") == "success":
                        st.success(f"User '{del_username}' deleted!")
                    else:
                        st.error(response.get("error_message", "Failed to delete user"))
                else:
                    st.warning("Please enter a username")
    
    with tab3:
        st.markdown("#### All Users")
        if st.button("Load Users", use_container_width=True):
            response = send_request("list_users", {})
            
            if response.get("status") == "success":
                users = response.get("data", {}).get("users", [])
                
                if users:
                    for user in users:
                        with st.container():
                            col1, col2, col3 = st.columns([2, 1, 1])
                            with col1:
                                st.write(f"**{user.get('username', 'N/A')}**")
                            with col2:
                                role_text = "Admin" if user.get('role') == 1 else "Normal"
                                st.write(f"Role: {role_text}")
                            with col3:
                                status = "Active" if user.get('is_active') else "Inactive"
                                st.write(f"Status: {status}")
                            st.markdown("---")
                else:
                    st.info("No users found")
            else:
                st.error(response.get("error_message", "Failed to load users"))

def system_info():
    """Display system information"""
    st.markdown("### ⚙️ System Information")
    
    col1, col2 = st.columns(2)
    
    with col1:
        if st.button("Get System Stats", use_container_width=True):
            response = send_request("system_info", {})
            
            if response.get("status") == "success":
                data = response.get("data", {})
                
                st.metric("Total Size", f"{data.get('total_size', 0) / (1024*1024):.2f} MB")
                st.metric("Used Space", f"{data.get('used_space', 0) / (1024*1024):.2f} MB")
                st.metric("Free Space", f"{data.get('free_space', 0) / (1024*1024):.2f} MB")
                st.metric("Total Files", data.get('total_files', 0))
                st.metric("Total Directories", data.get('total_directories', 0))
            else:
                st.error(response.get("error_message", "Failed to get stats"))
    
    with col2:
        if st.button("Get Session Info", use_container_width=True):
            response = send_request("get_session_info", {})
            
            if response.get("status") == "success":
                data = response.get("data", {})
                st.write(f"**Username:** {data.get('username', 'N/A')}")
                st.write(f"**Role:** {'Admin' if data.get('role') == 1 else 'Normal'}")
                st.write(f"**Login Time:** {datetime.fromtimestamp(data.get('login_time', 0)).strftime('%Y-%m-%d %H:%M:%S')}")
            else:
                st.error(response.get("error_message", "Failed to get session info"))

def main_interface():
    """Main application interface after login"""
    # Sidebar
    with st.sidebar:
        st.title("🗂️ OFS")
        st.markdown(f"**User:** {st.session_state.username}")
        st.markdown(f"**Session:** {st.session_state.session_id[:8]}...")
        st.markdown("---")
        
        page = st.radio("Navigation", 
                       ["📂 File Browser", "👥 Users", "⚙️ System Info"],
                       label_visibility="collapsed")
        
        st.markdown("---")
        
        if st.button("🚪 Logout", use_container_width=True):
            send_request("logout", {})
            st.session_state.logged_in = False
            st.session_state.session_id = ""
            st.session_state.username = ""
            st.session_state.current_path = "/"
            st.rerun()
    
    # Main content
    if page == "📂 File Browser":
        file_browser()
    elif page == "👥 Users":
        user_management()
    elif page == "⚙️ System Info":
        system_info()

def main():
    """Main application entry point"""
    st.set_page_config(
        page_title="OFS - Omni File System",
        page_icon="🗂️",
        layout="wide",
        initial_sidebar_state="expanded"
    )
    
    # Custom CSS
    st.markdown("""
        <style>
        .stButton > button {
            width: 100%;
        }
        </style>
    """, unsafe_allow_html=True)
    
    if not st.session_state.logged_in:
        login_page()
    else:
        main_interface()

if __name__ == "__main__":
    main()