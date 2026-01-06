// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include "InjectDll.hpp"

#include <Windows.h>
#include <psapi.h>

#include <wil/resource.h>

#include <print>

namespace fs = std::filesystem;

void InjectDll(const uint32_t processId, const std::filesystem::path& dllPath) {
  THROW_HR_IF(E_INVALIDARG, !fs::exists(dllPath));
  const std::wstring dllName = dllPath.filename().wstring();
  const std::wstring fullPath = fs::absolute(dllPath).wstring();

  const wil::unique_process_handle process(OpenProcess(
    PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE
      | PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD,
    FALSE,
    processId));
  THROW_LAST_ERROR_IF_NULL(process);

  // 3. Check if already injected
  HMODULE modules[1024] {};
  DWORD cbNeeded;
  if (EnumProcessModules(process.get(), modules, sizeof(modules), &cbNeeded)) {
    for (size_t i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
      wchar_t szModName[MAX_PATH];
      if (GetModuleBaseNameW(process.get(), modules[i], szModName, MAX_PATH)) {
        if (_wcsicmp(szModName, dllName.c_str()) == 0) {
          std::println(
            "DLL already loaded in process {}, skipping hijack", processId);
          return;
        }
      }
    }
  }

  size_t pathSize = (fullPath.length() + 1) * sizeof(wchar_t);
  void* remoteMemory = VirtualAllocEx(
    process.get(), nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  THROW_LAST_ERROR_IF_NULL(remoteMemory);

  const auto freeRemoteMemory = wil::scope_exit(
    [&] { VirtualFreeEx(process.get(), remoteMemory, 0, MEM_RELEASE); });

  // 5. Write the path to the allocated memory
  THROW_IF_WIN32_BOOL_FALSE(WriteProcessMemory(
    process.get(), remoteMemory, fullPath.c_str(), pathSize, nullptr));

  // 6. Execute LoadLibraryW in the target process
  const auto loadLibrary
    = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
  THROW_LAST_ERROR_IF_NULL(loadLibrary);

  const wil::unique_handle remoteThread(CreateRemoteThread(
    process.get(),
    nullptr,
    0,
    reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibrary),
    remoteMemory,
    0,
    nullptr));
  THROW_LAST_ERROR_IF_NULL(remoteThread);

  // Wait for LoadLibraryW to finish
  WaitForSingleObject(remoteThread.get(), INFINITE);
  DWORD exitCode {};
  GetExitCodeThread(remoteThread.get(), &exitCode);

  std::println("Injected {} into process {}", dllPath.filename().string(), processId);
}

void InjectDllByExecutableFileName(
  std::wstring_view executableFileName,
  const std::filesystem::path& dllPath) {
  std::vector<DWORD> pids(1024);
  DWORD cbNeeded = 0;

  while (true) {
    if (!EnumProcesses(pids.data(), static_cast<DWORD>(pids.size() * sizeof(DWORD)), &cbNeeded)) {
      THROW_LAST_ERROR();
    }
    if (cbNeeded < pids.size() * sizeof(DWORD)) break;
    pids.resize(pids.size() * 2);
  }

  const DWORD processCount = cbNeeded / sizeof(DWORD);
  bool found = false;

  for (DWORD i = 0; i < processCount; ++i) {
    if (pids[i] == 0) continue;

    wil::unique_process_handle hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pids[i]));
    if (!hProcess) continue;

    wchar_t szProcessName[MAX_PATH];
    HMODULE hMod;
    DWORD cbModNeeded;

    if (EnumProcessModules(hProcess.get(), &hMod, sizeof(hMod), &cbModNeeded)) {
      if (GetModuleBaseNameW(hProcess.get(), hMod, szProcessName, MAX_PATH)) {
        if (executableFileName == szProcessName) {
          InjectDll(pids[i], dllPath);
          found = true;
        }
      }
    }
  }

  if (!found) {
    // TODO: raise error
    std::println("Target process not found.");
  }
}