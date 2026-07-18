p = r"C:\Users\17410\Desktop\remote control\apps\agent\src\agent.cpp"
with open(p, "r", encoding="utf-8", errors="replace") as f:
    c = f.read()

# Add skip counter after the unchanged branch
old_skip = '''            if (changed) {'''
new_skip = '''            static int skipCount = 0, sendCount = 0;
            if (!changed) {
                skipCount++;
                if (skipCount <= 3 || skipCount % 100 == 0)
                    std::cerr << "[AGENT] SKIP #" << skipCount << " (send=" << sendCount << ")" << std::endl;
            }
            if (changed) {'''

c = c.replace(old_skip, new_skip)

# Add send counter increment
old_sendct = '''                static int frameCount = 0;
                static auto lastSendTime'''
new_sendct = '''                sendCount++;
                static int frameCount = 0;
                static auto lastSendTime'''

c = c.replace(old_sendct, new_sendct)

with open(p, "w", encoding="utf-8") as f:
    f.write(c)
print("Delta diagnostic added")