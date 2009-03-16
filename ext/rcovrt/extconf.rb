unless RUBY_PLATFORM == 'java' then
  require 'mkmf'

  if RUBY_VERSION =~ /1.9/
    $CFLAGS << ' -DRUBY_19_COMPATIBILITY'
    dir_config("gcov")
    if ENV["USE_GCOV"] and Config::CONFIG['CC'] =~ /gcc/ and
      have_library("gcov", "__gcov_open")
      $CFLAGS << " -fprofile-arcs -ftest-coverage"
      create_makefile("rcovrt")
    else
      
      create_makefile("rcovrt")
    end
  end
end
