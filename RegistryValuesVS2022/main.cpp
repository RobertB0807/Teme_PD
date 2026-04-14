#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static HKEY parse_root_key(const wchar_t *root_name) {
    if (_wcsicmp(root_name, L"HKCU") == 0 || _wcsicmp(root_name, L"HKEY_CURRENT_USER") == 0) {
        return HKEY_CURRENT_USER;
    }
    if (_wcsicmp(root_name, L"HKLM") == 0 || _wcsicmp(root_name, L"HKEY_LOCAL_MACHINE") == 0) {
        return HKEY_LOCAL_MACHINE;
    }
    if (_wcsicmp(root_name, L"HKCR") == 0 || _wcsicmp(root_name, L"HKEY_CLASSES_ROOT") == 0) {
        return HKEY_CLASSES_ROOT;
    }
    if (_wcsicmp(root_name, L"HKU") == 0 || _wcsicmp(root_name, L"HKEY_USERS") == 0) {
        return HKEY_USERS;
    }
    if (_wcsicmp(root_name, L"HKCC") == 0 || _wcsicmp(root_name, L"HKEY_CURRENT_CONFIG") == 0) {
        return HKEY_CURRENT_CONFIG;
    }
    return NULL;
}

static const wchar_t *registry_type_name(DWORD type) {
    switch (type) {
        case REG_NONE: return L"REG_NONE";
        case REG_SZ: return L"REG_SZ";
        case REG_EXPAND_SZ: return L"REG_EXPAND_SZ";
        case REG_BINARY: return L"REG_BINARY";
        case REG_DWORD: return L"REG_DWORD";
        case REG_DWORD_BIG_ENDIAN: return L"REG_DWORD_BIG_ENDIAN";
        case REG_LINK: return L"REG_LINK";
        case REG_MULTI_SZ: return L"REG_MULTI_SZ";
        case REG_RESOURCE_LIST: return L"REG_RESOURCE_LIST";
        case REG_FULL_RESOURCE_DESCRIPTOR: return L"REG_FULL_RESOURCE_DESCRIPTOR";
        case REG_RESOURCE_REQUIREMENTS_LIST: return L"REG_RESOURCE_REQUIREMENTS_LIST";
        case REG_QWORD: return L"REG_QWORD";
        default: return L"UNKNOWN";
    }
}

static void print_hex_bytes(const BYTE *data, DWORD size) {
    DWORD i;
    for (i = 0; i < size; ++i) {
        wprintf(L"%02X", data[i]);
        if (i + 1 < size) {
            wprintf(L" ");
        }
    }
}

static void print_reg_sz(const BYTE *data, DWORD size) {
    const wchar_t *text = (const wchar_t *)data;
    size_t chars = size / sizeof(wchar_t);

    if (chars == 0) {
        wprintf(L"(empty)");
        return;
    }

    if (text[chars - 1] == L'\0') {
        chars--;
    }

    wprintf(L"%.*ls", (int)chars, text);
}

static void print_reg_multi_sz(const BYTE *data, DWORD size) {
    const wchar_t *p = (const wchar_t *)data;
    size_t remaining = size / sizeof(wchar_t);
    int first = 1;

    while (remaining > 0 && *p != L'\0') {
        size_t len = wcslen(p);
        if (len >= remaining) {
            break;
        }
        if (!first) {
            wprintf(L" | ");
        }
        wprintf(L"%ls", p);
        first = 0;
        p += len + 1;
        remaining -= len + 1;
    }

    if (first) {
        wprintf(L"(empty)");
    }
}

static void print_value_data(DWORD type, const BYTE *data, DWORD size) {
    switch (type) {
        case REG_SZ:
        case REG_EXPAND_SZ:
            print_reg_sz(data, size);
            break;

        case REG_MULTI_SZ:
            print_reg_multi_sz(data, size);
            break;

        case REG_DWORD:
            if (size >= sizeof(DWORD)) {
                DWORD v = *(const DWORD *)data;
                wprintf(L"%lu (0x%08lX)", v, v);
            } else {
                wprintf(L"(invalid DWORD size)");
            }
            break;

        case REG_DWORD_BIG_ENDIAN:
            if (size >= sizeof(DWORD)) {
                DWORD v = ((DWORD)data[0] << 24) | ((DWORD)data[1] << 16) | ((DWORD)data[2] << 8) | (DWORD)data[3];
                wprintf(L"%lu (0x%08lX)", v, v);
            } else {
                wprintf(L"(invalid DWORD big-endian size)");
            }
            break;

        case REG_QWORD:
            if (size >= sizeof(ULONGLONG)) {
                ULONGLONG v = *(const ULONGLONG *)data;
                wprintf(L"%I64u (0x%016I64X)", v, v);
            } else {
                wprintf(L"(invalid QWORD size)");
            }
            break;

        case REG_BINARY:
        default:
            print_hex_bytes(data, size);
            break;
    }
}

int wmain(int argc, wchar_t *argv[]) {
    HKEY root;
    HKEY key = NULL;
    LONG status;
    DWORD value_count = 0;
    DWORD max_value_name_len = 0;
    DWORD max_value_data_len = 0;
    wchar_t *value_name = NULL;
    BYTE *value_data = NULL;
    DWORD i;

    if (argc != 3) {
        wprintf(L"Usage:\n");
        wprintf(L"  %ls <ROOT_KEY> <SUBKEY_PATH>\n\n", argv[0]);
        wprintf(L"Valid ROOT_KEY: HKCU, HKLM, HKCR, HKU, HKCC\n");
        wprintf(L"Example: %ls HKCU Software\\Microsoft\\Windows\\CurrentVersion\\Run\n", argv[0]);
        return 1;
    }

    root = parse_root_key(argv[1]);
    if (root == NULL) {
        wprintf(L"Invalid root key: %ls\n", argv[1]);
        return 1;
    }

    status = RegOpenKeyExW(root, argv[2], 0, KEY_READ, &key);
    if (status != ERROR_SUCCESS) {
        wprintf(L"RegOpenKeyExW failed with code %ld\n", status);
        return 1;
    }

    status = RegQueryInfoKeyW(
        key,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        &value_count,
        &max_value_name_len,
        &max_value_data_len,
        NULL,
        NULL
    );

    if (status != ERROR_SUCCESS) {
        wprintf(L"RegQueryInfoKeyW failed with code %ld\n", status);
        RegCloseKey(key);
        return 1;
    }

    value_name = (wchar_t *)malloc((max_value_name_len + 1) * sizeof(wchar_t));
    value_data = (BYTE *)malloc(max_value_data_len > 0 ? max_value_data_len : 1);
    if (value_name == NULL || value_data == NULL) {
        wprintf(L"Out of memory\n");
        free(value_name);
        free(value_data);
        RegCloseKey(key);
        return 1;
    }

    wprintf(L"Subkey: %ls\\%ls\n", argv[1], argv[2]);
    wprintf(L"Values: %lu\n\n", value_count);

    for (i = 0; i < value_count; ++i) {
        DWORD name_len = max_value_name_len + 1;
        DWORD data_len = max_value_data_len;
        DWORD type = 0;

        status = RegEnumValueW(key, i, value_name, &name_len, NULL, &type, value_data, &data_len);
        if (status == ERROR_MORE_DATA) {
            BYTE *new_data = (BYTE *)realloc(value_data, data_len);
            if (new_data == NULL) {
                wprintf(L"Cannot reallocate value_data buffer\n");
                break;
            }
            value_data = new_data;
            status = RegEnumValueW(key, i, value_name, &name_len, NULL, &type, value_data, &data_len);
        }

        if (status != ERROR_SUCCESS) {
            wprintf(L"RegEnumValueW failed at index %lu with code %ld\n\n", i, status);
            continue;
        }

        wprintf(L"Value #%lu\n", i + 1);
        if (name_len == 0) {
            wprintf(L"  Name: (Default)\n");
        } else {
            wprintf(L"  Name: %ls\n", value_name);
        }
        wprintf(L"  Type: %ls\n", registry_type_name(type));
        wprintf(L"  Data: ");
        print_value_data(type, value_data, data_len);
        wprintf(L"\n\n");
    }

    free(value_name);
    free(value_data);
    RegCloseKey(key);
    return 0;
}