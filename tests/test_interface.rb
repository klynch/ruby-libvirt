#!/usr/bin/ruby

# Test the interface methods the bindings support

$: << File.dirname(__FILE__)

require 'libvirt'
require 'test_utils.rb'

def find_valid_iface(conn)
  conn.list_interfaces.each do |ifname|
    iface = conn.lookup_interface_by_name(ifname)
    if iface.mac == "00:00:00:00:00:00"
      next
    end
    return iface
  end
end

conn = Libvirt::open("qemu:///system")

# test setup
begin
  `rm -f /etc/sysconfig/network-scripts/ifcfg-ruby-libvirt-tester`
  `brctl delbr ruby-libvirt-tester >& /dev/null`
rescue
end

# TESTGROUP: iface.undefine
newiface = conn.define_interface_xml($new_interface_xml)

expect_too_many_args(newiface, "undefine", 1)

expect_success(newiface, "no args", "undefine")

# TESTGROUP: iface.create
newiface = conn.define_interface_xml($new_interface_xml)

expect_too_many_args(newiface, "create", 1, 2)
expect_invalid_arg_type(newiface, "create", 'foo')

#expect_success(newiface, "no args", "create")

#expect_fail(newiface, Libvirt::Error, "on already running interface", "create")

#newiface.destroy
newiface.undefine

# TESTGROUP: iface.destroy
newiface = conn.define_interface_xml($new_interface_xml)

expect_too_many_args(newiface, "destroy", 1, 2)
expect_invalid_arg_type(newiface, "destroy", 'foo')

expect_success(newiface, "no args", "destroy")

newiface.undefine

# TESTGROUP: iface.active?
newiface = conn.define_interface_xml($new_interface_xml)

expect_too_many_args(newiface, "active?", 1)

expect_success(newiface, "no args", "active?") {|x| x == false}

#newiface.create
#expect_success(newiface, "no args", "active?") {|x| x == true}

#newiface.destroy
newiface.undefine

# TESTGROUP: iface.name
newiface = conn.define_interface_xml($new_interface_xml)

expect_too_many_args(newiface, "name", 1)

expect_success(newiface, "no args", "name") {|x| x == "ruby-libvirt-tester"}

newiface.undefine

# TESTGROUP: iface.mac
testiface = find_valid_iface(conn)
if not testiface.nil?
  expect_too_many_args(testiface, "mac", 1)

  expect_success(testiface, "no args", "mac") {|x| x == testiface.mac}
end

# TESTGROUP: iface.xml_desc
testiface = find_valid_iface(conn)
if not testiface.nil?
  expect_too_many_args(testiface, "xml_desc", 1, 2)
  expect_invalid_arg_type(testiface, "xml_desc", "foo")
  expect_success(testiface, "no args", "xml_desc")
end

# TESTGROUP: iface.free
newiface = conn.define_interface_xml($new_interface_xml)
newiface.undefine
expect_too_many_args(newiface, "free", 1)

expect_success(newiface, "no args", "free")

conn.close

finish_tests
