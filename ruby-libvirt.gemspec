PKG_NAME='ruby-libvirt'
PKG_VERSION='0.4.0'
                      
PKG_FILES = [ "Rakefile", "COPYING", "README", "NEWS", "README.rdoc"] + Dir.glob("{lib,ext,tests,spec}/**/*")
                      

SPEC = Gem::Specification.new do |s|
    s.name = PKG_NAME
    s.version = PKG_VERSION
    s.email = "libvir-list@redhat.com"
    s.homepage = "http://libvirt.org/ruby/"
    s.summary = "Ruby bindings for LIBVIRT"
    s.files = PKG_FILES
    s.required_ruby_version = '>= 1.8.1'
    s.extensions = "ext/libvirt/extconf.rb"
    s.author = "David Lutterkort, Chris Lalancette"
    s.rubyforge_project = "None"
    s.description = "Ruby bindings for libvirt."
end
