
# Phrase 1

```c
ESP32 START
    |
    v
Khởi tạo I2C
    |
    v
Khởi tạo DS3231
    |
    v
Kiểm tra DS3231
```

# Phrase 2
```c
Khởi tạo WiFi
    |
    v
Kết nối Router
    |
    v
Có WiFi không?
    |
    +---- YES ----> Lấy thời gian NTP
    |                   |
    |                   v
    |             NTP thành công?
    |                   |
    |              +----+----+
    |              |         |
    |             YES        NO
    |              |         |
    |              v         |
    |        Ghi NTP vào RTC |
    |              |         |
    |              +----+----+
    |                   |
    +---- NO -----------+
                        |
                        v
                 Đọc thời gian RTC
```

# Phrase 3
```c
Đến chu kỳ đo
      |
      v
Đọc dữ liệu cảm biến
      |
      v
Đọc thời gian DS3231
      |
      v
Tạo bản ghi dữ liệu
      |
      v
Gắn timestamp
      |
      v
Gửi lên ThingsBoard
```