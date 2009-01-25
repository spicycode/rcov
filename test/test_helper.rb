require 'test/unit'
require 'pathname'
require 'fileutils'
require File.expand_path(File.join(File.dirname(__FILE__), '..', 'lib', 'rcov'))


def get_ruby_bin
  @ruby_version ||= ENV['ENV_RUBY'] || 'ruby'
end
