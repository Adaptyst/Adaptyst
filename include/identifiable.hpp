#ifndef IDENTIFIABLE_HPP_
#define IDENTIFIABLE_HPP_

#include <vector>
#include <string>

namespace adaptyst {
  class Identifiable {
  private:
    std::string id;

  protected:
    Identifiable(std::string id);

  public:
    std::string get_id();
    virtual std::vector<std::string> get_log_types() = 0;
    virtual std::string get_type() = 0;
  };
}

#endif
