// Copyright 2026 Fred Emmott <fred@fredemmott.com>
// SPDX-License-Identifier: MIT

#include "InjectDll.hpp"

#include <Windows.h>
#include <psapi.h>
#include <winternl.h>

#include <ntstatus.h>
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
  if ((!process) && GetLastError() == ERROR_ACCESS_DENIED) {
    throw std::runtime_error("Windows says you don't have permission to hijack the driver process; try running as administrator.");
  }
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

  std::println(
    "Injected {} into process {}", dllPath.filename().string(), processId);
}

SYSTEM_PROCESS_INFORMATION* next(SYSTEM_PROCESS_INFORMATION* it) {
  if (!it->NextEntryOffset) {
    return nullptr;
  }

  return reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(
    reinterpret_cast<uintptr_t>(it) + it->NextEntryOffset);
}

void InjectDllByExecutableFileName(
  std::wstring_view executableFileName,
  const std::filesystem::path& dllPath) {
  std::string processInfoBuffer(
    sizeof(SYSTEM_PROCESS_INFORMATION) * 1024, '\0');
  ULONG infoByteCount {processInfoBuffer.size()};
  while (NtQuerySystemInformation(
           SystemProcessInformation,
           processInfoBuffer.data(),
           infoByteCount,
           &infoByteCount)
         == STATUS_INFO_LENGTH_MISMATCH) {
    infoByteCount += sizeof(SYSTEM_PROCESS_INFORMATION) * 32;
    processInfoBuffer.resize(infoByteCount);
  }

  for (auto process = reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(
         processInfoBuffer.data());
       process->NextEntryOffset;
       process = next(process)) {
    const auto& ntName = process->ImageName;
    if (!(ntName.Buffer && ntName.Length)) {
      continue;
    }
    const std::wstring_view name { ntName.Buffer, ntName.Length / sizeof(wchar_t) };
    if (name == executableFileName) {
      const auto pid = static_cast<DWORD>(std::bit_cast<uintptr_t>(process->UniqueProcessId));
      InjectDll(pid, dllPath);
      return;
    }
  }

  throw std::runtime_error("Could not find target driver process to inject");
}