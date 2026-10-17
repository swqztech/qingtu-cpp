#include "dialogs.h"

#include <windows.h>
#include <shobjidl.h>

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static std::string wide_to_utf8(const std::wstring& w) {
  if (w.empty()) return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (n <= 0) return {};
  std::string s(static_cast<size_t>(n - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
  return s;
}

bool pick_image_files(std::vector<fs::path>& out) {
  IFileOpenDialog* dlg = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
    return false;
  DWORD opts = 0;
  dlg->GetOptions(&opts);
  dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_ALLOWMULTISELECT | FOS_FILEMUSTEXIST);
  COMDLG_FILTERSPEC filters[] = {{L"图片", L"*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.gif"}};
  dlg->SetFileTypes(1, filters);
  if (FAILED(dlg->Show(nullptr))) {
    dlg->Release();
    return false;
  }
  IShellItemArray* items = nullptr;
  if (FAILED(dlg->GetResults(&items))) {
    dlg->Release();
    return false;
  }
  DWORD count = 0;
  items->GetCount(&count);
  for (DWORD i = 0; i < count; ++i) {
    IShellItem* item = nullptr;
    if (FAILED(items->GetItemAt(i, &item))) continue;
    PWSTR path = nullptr;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
      out.emplace_back(path);
      CoTaskMemFree(path);
    }
    item->Release();
  }
  items->Release();
  dlg->Release();
  return !out.empty();
}

bool pick_output_folder(std::string& utf8_path) {
  IFileOpenDialog* dlg = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
    return false;
  DWORD opts = 0;
  dlg->GetOptions(&opts);
  dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_PICKFOLDERS);
  if (FAILED(dlg->Show(nullptr))) {
    dlg->Release();
    return false;
  }
  IShellItem* item = nullptr;
  if (FAILED(dlg->GetResult(&item))) {
    dlg->Release();
    return false;
  }
  PWSTR path = nullptr;
  if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
    utf8_path = wide_to_utf8(path);
    CoTaskMemFree(path);
  }
  item->Release();
  dlg->Release();
  return !utf8_path.empty();
}
