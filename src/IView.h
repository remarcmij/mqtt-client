#pragma once

struct ViewModel;

class IView {
public:
  virtual void update(const ViewModel &viewModel) = 0;
};
