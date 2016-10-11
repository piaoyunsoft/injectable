# Injectable
injectable is a tiny tool for iOS Jailbreak Development

It removes PIE and disable __RESTRICT segment to allow you using cycript or DYLD_INSERT_LIBRARIES or injecting delis via MobileSubstrate

Please see my blog post [Jailbreak Development please go easy on me ](https://blog.0xbbc.com/2016/10/jailbreak-development-please-go-easy-on-me/) for details

### Usage

```injectable [MachO file]```

If the file is a valid MachO file, then you will see a new file with ```.injectable``` at the end of the original filename. 

### Screenshots
cycript failed with the original executable file
![Screenshots-1](https://raw.githubusercontent.com/BlueCocoa/injectable/master/screenshot-1.png)

using injectable
![Screenshots-2](https://raw.githubusercontent.com/BlueCocoa/injectable/master/screenshot-2.png)

cycript works
![Screenshots-3](https://raw.githubusercontent.com/BlueCocoa/injectable/master/screenshot-3.png)

reveal and dylibs would work, too
![Screenshots-4](https://raw.githubusercontent.com/BlueCocoa/injectable/master/screenshot-4.png)
