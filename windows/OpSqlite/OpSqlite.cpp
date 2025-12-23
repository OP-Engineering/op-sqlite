#include "pch.h"

#include "OpSqlite.h"

namespace winrt::OpSqlite
{

// See https://microsoft.github.io/react-native-windows/docs/native-platform for help writing native modules

void OpSqlite::Initialize(React::ReactContext const &reactContext) noexcept {
  m_context = reactContext;
}

double OpSqlite::multiply(double a, double b) noexcept {
  return a * b;
}

} // namespace winrt::OpSqlite