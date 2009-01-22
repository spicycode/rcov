require 'test/unit'
require 'rcov'
require 'pathname'
require 'fileutils'


def get_ruby_bin
  # if defined? PLATFORM && PLATFORM == 'java'
  #   @@ruby_bin ||= 'jruby'
  # else
  #   @@ruby_bin ||= `ps -A | grep #{$$}`.match(/(\/[^\s]+)/)[0]
  # end
  'ruby'
end