$FAIL = 0
$SUCCESS = 0
$SKIPPED = 0

$GUEST_DISK = '/var/lib/libvirt/images/ruby-libvirt-tester.qcow2'
$GUEST_UUID = "93a5c045-6457-2c09-e56f-927cdf34e17a"

# XML data for later tests
$new_dom_xml = <<EOF
<domain type='kvm'>
  <name>ruby-libvirt-tester</name>
  <uuid>#{$GUEST_UUID}</uuid>
  <memory>1048576</memory>
  <currentMemory>1048576</currentMemory>
  <vcpu>2</vcpu>
  <os>
    <type arch='x86_64'>hvm</type>
    <boot dev='hd'/>
  </os>
  <features>
    <acpi/>
    <apic/>
    <pae/>
  </features>
  <clock offset='utc'/>
  <on_poweroff>destroy</on_poweroff>
  <on_reboot>restart</on_reboot>
  <on_crash>restart</on_crash>
  <devices>
    <disk type='file' device='disk'>
      <driver name='qemu' type='qcow2'/>
      <source file='#{$GUEST_DISK}'/>
      <target dev='vda' bus='virtio'/>
    </disk>
    <interface type='bridge'>
      <mac address='52:54:00:60:3c:95'/>
      <source bridge='virbr0'/>
      <model type='virtio'/>
      <target dev='rl556'/>
    </interface>
    <serial type='pty'>
      <target port='0'/>
    </serial>
    <console type='pty'>
      <target port='0'/>
    </console>
    <input type='mouse' bus='ps2'/>
    <graphics type='vnc' port='-1' autoport='yes' keymap='en-us'/>
    <video>
      <model type='cirrus' vram='9216' heads='1'/>
    </video>
  </devices>
</domain>
EOF

# qemu command-line that roughly corresponds to the above XML
$qemu_cmd_line = "/usr/bin/qemu-kvm -S -M pc-0.13 -enable-kvm -m 1024 -smp 1,sockets=1,cores=1,threads=1 -name ruby-libvirt-tester -uuid #{$GUEST_UUID} -nodefconfig -nodefaults -chardev socket,id=monitor,path=/var/lib/libvirt/qemu/ruby-libvirt-tester.monitor,server,nowait -mon chardev=monitor,mode=readline -rtc base=utc -boot c -chardev pty,id=serial0 -device isa-serial,chardev=serial0 -usb -vnc 127.0.0.1:0 -k en-us -vga cirrus -device virtio-balloon-pci,id=balloon0,bus=pci.0,addr=0x5"

$new_interface_xml = <<EOF
<interface type="bridge" name="ruby-libvirt-tester">
  <start mode="onboot"/>
  <bridge delay="0">
  </bridge>
</interface>
EOF

$NETWORK_UUID = "04068860-d9a2-47c5-bc9d-9e047ae901da"
$new_net_xml = <<EOF
<network>
  <name>ruby-libvirt-tester</name>
  <uuid>#{$NETWORK_UUID}</uuid>
  <forward mode='nat'/>
  <bridge name='rubybr0' stp='on' delay='0' />
  <ip address='192.168.134.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='192.168.134.2' end='192.168.134.254' />
    </dhcp>
  </ip>
</network>
EOF

$NWFILTER_UUID = "bd339530-134c-6d07-441a-17fb90dad807"
$new_nwfilter_xml = <<EOF
<filter name='ruby-libvirt-tester' chain='ipv4'>
  <uuid>#{$NWFILTER_UUID}</uuid>
  <rule action='accept' direction='out' priority='100'>
    <ip srcipaddr='0.0.0.0' dstipaddr='255.255.255.255' protocol='tcp' srcportstart='63000' dstportstart='62000'/>
  </rule>
  <rule action='accept' direction='in' priority='100'>
    <ip protocol='tcp' srcportstart='63000' dstportstart='62000'/>
  </rule>
</filter>
EOF

$SECRET_UUID = "bd339530-134c-6d07-4410-17fb90dad805"
$new_secret_xml = <<EOF
<secret ephemeral='no' private='no'>
  <description>test secret</description>
  <uuid>#{$SECRET_UUID}</uuid>
  <usage type='volume'>
    <volume>/var/lib/libvirt/images/mail.img</volume>
  </usage>
</secret>
EOF

$POOL_UUID = "33a5c045-645a-2c00-e56b-927cdf34e17a"
$POOL_PATH = "/var/lib/libvirt/images/ruby-libvirt-tester"
$new_storage_pool_xml = <<EOF
<pool type="dir">
  <name>ruby-libvirt-tester</name>
  <uuid>#{$POOL_UUID}</uuid>
  <target>
    <path>#{$POOL_PATH}</path>
  </target>
</pool>
EOF

def expect_success(object, msg, func, *args)
  begin
    x = object.send(func, *args)
    if block_given?
      res = yield x
      if not res
        # FIXME: generate a proper error here
        raise "Failed"
      end
    end
    puts_ok "#{func} #{msg} succeeded"
    x
  rescue NoMethodError
    puts_skipped "#{func} does not exist"
  rescue => e
    puts_fail "#{func} #{msg} expected to succeed, threw #{e.class.to_s}: #{e.to_s}"
  end
end

def expect_fail(object, errtype, errmsg, func, *args)
  begin
    object.send(func, *args)
  rescue NoMethodError
    puts_skipped "#{func} does not exist"
  rescue errtype => e
    puts_ok "#{func} #{errmsg} threw #{errtype.to_s}"
  rescue => e
    puts_fail "#{func} #{errmsg} expected to throw #{errtype.to_s}, but instead threw #{e.class.to_s}: #{e.to_s}"
  else
    puts_fail "#{func} #{errmsg} expected to throw #{errtype.to_s}, but threw nothing"
  end
end

def expect_too_many_args(object, func, *args)
  expect_fail(object, ArgumentError, "too many args", func, *args)
end

def expect_too_few_args(object, func, *args)
  expect_fail(object, ArgumentError, "too few args", func, *args)
end

def expect_invalid_arg_type(object, func, *args)
  expect_fail(object, TypeError, "invalid arg type", func, *args)
end

def puts_ok(str)
  puts "OK: " + str
  $SUCCESS = $SUCCESS + 1
end

def puts_fail(str)
  puts "FAIL: " + str
  $FAIL = $FAIL + 1
end

def puts_skipped(str)
  puts "SKIPPED: " + str
  $SKIPPED = $SKIPPED + 1
end

def finish_tests
  puts "Successfully finished #{$SUCCESS} tests, failed #{$FAIL} tests, skipped #{$SKIPPED} tests"
end
