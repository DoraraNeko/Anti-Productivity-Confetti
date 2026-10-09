#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <random>
#include <string>
#include <vector>

namespace
{
    constexpr wchar_t kWindowClass[] = L"APCOverlayWindow";
    constexpr wchar_t kAppTitle[] = L"Anti-Productivity Confetti (APC)";
    constexpr UINT_PTR kFrameTimerId = 1;
    constexpr UINT kConfettiMessage = WM_APP + 1;
    constexpr UINT kSelectCrackerStyleMessage = WM_APP + 2;
    constexpr WORD kIconResourceId = 1;
    constexpr ULONGLONG kInputCooldownMs = 80;
    constexpr float kPi = 3.14159265358979323846f;
    constexpr wchar_t kSettingsFileName[] = L"settings.ini";

    struct Point
    {
        float x;
        float y;
    };

    struct MonitorBounds
    {
        float left;
        float top;
        float right;
        float bottom;
    };

    struct Confetti
    {
        float x = 0.0f;
        float y = 0.0f;
        float velocityX = 0.0f;
        float velocityY = 0.0f;
        float sway = 0.0f;
        float phase = 0.0f;
        float rotation = 0.0f;
        float rotationSpeed = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        float age = 0.0f;
        float lifetime = 0.0f;
        float gravity = 0.0f;
        BYTE red = 255;
        BYTE green = 255;
        BYTE blue = 255;
        bool burst = false;
    };

    struct Settings
    {
        UINT rainIntervalMs = 50;
        UINT piecesPerCracker = 55;
        UINT crackerVisibleMs = 1000;
        UINT crackerStyle = 3;
        size_t maxConfetti = 10000;
    };

    HINSTANCE gInstance = nullptr;
    HWND gWindow = nullptr;
    HHOOK gKeyboardHook = nullptr;
    HHOOK gMouseHook = nullptr;
    HDC gMemoryDc = nullptr;
    HBITMAP gBitmap = nullptr;
    HBITMAP gPreviousBitmap = nullptr;
    BYTE* gPixels = nullptr;
    int gScreenX = 0;
    int gScreenY = 0;
    int gWidth = 0;
    int gHeight = 0;
    std::wstring gSettingsPath;
    Settings gSettings;
    bool gPressedKeys[256]{};
    bool gRunning = true;
    ULONGLONG gLastInput = 0;
    ULONGLONG gLastFrame = 0;
    ULONGLONG gLastSpawn = 0;
    ULONGLONG gCrackerVisibleUntil = 0;
    size_t gNextBurstStart = 0;
    UINT gCurrentCrackerStyle = 3;
    std::vector<Confetti> gConfetti;
    std::vector<MonitorBounds> gMonitors;
    std::mt19937 gRandom{ std::random_device{}() };
    const BYTE gPalette[][3] = {
        { 255, 68, 88 }, { 255, 190, 48 }, { 50, 220, 145 },
        { 60, 170, 255 }, { 196, 92, 255 }, { 255, 110, 190 },
        { 255, 244, 112 }
    };

    float RandomFloat(float minimum, float maximum)
    {
        return std::uniform_real_distribution<float>(minimum, maximum)(gRandom);
    }

    size_t RandomColor()
    {
        return std::uniform_int_distribution<size_t>(0, ARRAYSIZE(gPalette) - 1)(gRandom);
    }

    std::wstring GetExecutableDirectory()
    {
        wchar_t path[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
        if (length == 0 || length >= ARRAYSIZE(path))
        {
            return L"";
        }
        std::wstring directory(path, length);
        const size_t separator = directory.find_last_of(L"\\/");
        if (separator == std::wstring::npos)
        {
            return L".";
        }
        directory.resize(separator);
        return directory;
    }

    bool ReadBoundedSetting(const wchar_t* key, UINT defaultValue, UINT minimum, UINT maximum, UINT& result)
    {
        wchar_t value[64]{};
        GetPrivateProfileStringW(L"Confetti", key, L"", value, ARRAYSIZE(value), gSettingsPath.c_str());
        if (value[0] == L'\0')
        {
            result = defaultValue;
            return true;
        }
        wchar_t* end = nullptr;
        const unsigned long parsed = std::wcstoul(value, &end, 10);
        if (end == value || *end != L'\0' || parsed < minimum || parsed > maximum)
        {
            result = defaultValue;
            return false;
        }
        result = static_cast<UINT>(parsed);
        return true;
    }

    void LoadSettings()
    {
        if (gSettingsPath.empty())
        {
            return;
        }
        Settings loaded;
        UINT maxConfetti = static_cast<UINT>(loaded.maxConfetti);
        const bool validInterval = ReadBoundedSetting(L"RainIntervalMs", loaded.rainIntervalMs, 20, 1000, loaded.rainIntervalMs);
        const bool validBurstCount = ReadBoundedSetting(L"PiecesPerCracker", loaded.piecesPerCracker, 1, 200, loaded.piecesPerCracker);
        const bool validCrackerDuration = ReadBoundedSetting(
            L"CrackerVisibleMs", loaded.crackerVisibleMs, 100, 2000, loaded.crackerVisibleMs);
        const bool validCrackerStyle = ReadBoundedSetting(
            L"CrackerStyle", loaded.crackerStyle, 1, 3, loaded.crackerStyle);
        const bool validMaxConfetti = ReadBoundedSetting(L"MaxConfetti", maxConfetti, 50, 20000, maxConfetti);
        loaded.maxConfetti = maxConfetti;
        gSettings = loaded;
        gCurrentCrackerStyle = loaded.crackerStyle;
        if (!validInterval || !validBurstCount || !validCrackerDuration ||
            !validCrackerStyle || !validMaxConfetti)
        {
            MessageBoxW(gWindow,
                L"One or more settings in settings.ini are invalid. Defaults are used for invalid values.",
                kAppTitle, MB_OK | MB_ICONWARNING);
        }
    }

    bool EnsureSettingsFile()
    {
        gSettingsPath = GetExecutableDirectory();
        if (gSettingsPath.empty())
        {
            return false;
        }
        gSettingsPath += L"\\";
        gSettingsPath += kSettingsFileName;
        if (GetFileAttributesW(gSettingsPath.c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            return true;
        }
        const wchar_t defaults[] =
            L"[Confetti]\r\n"
            L"; Time between rain pieces in milliseconds (20-1000)\r\n"
            L"RainIntervalMs=50\r\n"
            L"; Pieces launched from each corner for each input (1-200)\r\n"
            L"PiecesPerCracker=55\r\n"
            L"; How long cracker illustrations stay visible after input in milliseconds (100-2000)\r\n"
            L"CrackerVisibleMs=1000\r\n"
            L"; Initial cracker design: 1=Paper cone, 2=Foil tube, 3=Pop/spring (1-3)\r\n"
            L"CrackerStyle=3\r\n"
            L"; Maximum live pieces (50-20000)\r\n"
            L"MaxConfetti=10000\r\n";
        DWORD written = 0;
        HANDLE file = CreateFileW(gSettingsPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return GetLastError() == ERROR_FILE_EXISTS;
        }
        const DWORD bytes = static_cast<DWORD>((ARRAYSIZE(defaults) - 1) * sizeof(wchar_t));
        const BOOL success = WriteFile(file, defaults, bytes, &written, nullptr);
        CloseHandle(file);
        return success && written == bytes;
    }

    void BlendPixel(int x, int y, BYTE red, BYTE green, BYTE blue, BYTE alpha)
    {
        if (x < 0 || x >= gWidth || y < 0 || y >= gHeight || alpha == 0)
        {
            return;
        }

        BYTE* pixel = gPixels + (static_cast<size_t>(y) * gWidth + x) * 4;
        const unsigned int inverseAlpha = 255u - alpha;
        pixel[0] = static_cast<BYTE>(static_cast<unsigned int>(blue) * alpha / 255u +
            pixel[0] * inverseAlpha / 255u);
        pixel[1] = static_cast<BYTE>(static_cast<unsigned int>(green) * alpha / 255u +
            pixel[1] * inverseAlpha / 255u);
        pixel[2] = static_cast<BYTE>(static_cast<unsigned int>(red) * alpha / 255u +
            pixel[2] * inverseAlpha / 255u);
        pixel[3] = static_cast<BYTE>(alpha + pixel[3] * inverseAlpha / 255u);
    }

    void FillPolygon(const Point* points, size_t count, BYTE red, BYTE green, BYTE blue, BYTE alpha)
    {
        if (!gPixels || count < 3)
        {
            return;
        }

        float minX = points[0].x;
        float maxX = minX;
        float minY = points[0].y;
        float maxY = minY;
        for (size_t index = 1; index < count; ++index)
        {
            minX = std::min(minX, points[index].x);
            maxX = std::max(maxX, points[index].x);
            minY = std::min(minY, points[index].y);
            maxY = std::max(maxY, points[index].y);
        }

        const int left = std::max(0, static_cast<int>(std::floor(minX)));
        const int right = std::min(gWidth - 1, static_cast<int>(std::ceil(maxX)));
        const int top = std::max(0, static_cast<int>(std::floor(minY)));
        const int bottom = std::min(gHeight - 1, static_cast<int>(std::ceil(maxY)));
        for (int y = top; y <= bottom; ++y)
        {
            for (int x = left; x <= right; ++x)
            {
                const float sampleX = static_cast<float>(x) + 0.5f;
                const float sampleY = static_cast<float>(y) + 0.5f;
                bool inside = false;
                for (size_t current = 0, previous = count - 1; current < count; previous = current++)
                {
                    const Point& a = points[current];
                    const Point& b = points[previous];
                    if ((a.y > sampleY) != (b.y > sampleY) &&
                        sampleX < (b.x - a.x) * (sampleY - a.y) / (b.y - a.y) + a.x)
                    {
                        inside = !inside;
                    }
                }
                if (inside)
                {
                    BlendPixel(x, y, red, green, blue, alpha);
                }
            }
        }
    }

    void FillEllipse(Point center, Point direction, Point normal, float radiusX, float radiusY,
        BYTE red, BYTE green, BYTE blue, BYTE alpha)
    {
        constexpr size_t segments = 20;
        Point points[segments]{};
        for (size_t index = 0; index < segments; ++index)
        {
            const float angle = 2.0f * kPi * static_cast<float>(index) / segments;
            points[index] = {
                center.x + normal.x * std::cos(angle) * radiusX + direction.x * std::sin(angle) * radiusY,
                center.y + normal.y * std::cos(angle) * radiusX + direction.y * std::sin(angle) * radiusY
            };
        }
        FillPolygon(points, segments, red, green, blue, alpha);
    }

    void DrawConfetti(const Confetti& piece, BYTE alpha)
    {
        const float cosine = std::cos(piece.rotation);
        const float sine = std::sin(piece.rotation);
        const float halfWidth = piece.width * 0.5f;
        const float halfHeight = piece.height * 0.5f;
        const Point corners[] = {
            { piece.x - halfWidth * cosine + halfHeight * sine, piece.y - halfWidth * sine - halfHeight * cosine },
            { piece.x + halfWidth * cosine + halfHeight * sine, piece.y + halfWidth * sine - halfHeight * cosine },
            { piece.x + halfWidth * cosine - halfHeight * sine, piece.y + halfWidth * sine + halfHeight * cosine },
            { piece.x - halfWidth * cosine - halfHeight * sine, piece.y - halfWidth * sine + halfHeight * cosine }
        };
        FillPolygon(corners, ARRAYSIZE(corners), piece.red, piece.green, piece.blue, alpha);
    }

    void SpawnFallingConfetti()
    {
        if (gConfetti.size() >= gSettings.maxConfetti)
        {
            return;
        }

        const auto& color = gPalette[RandomColor()];
        Confetti piece{};
        piece.x = RandomFloat(0.0f, static_cast<float>(gWidth));
        piece.y = RandomFloat(-36.0f, -5.0f);
        piece.velocityX = RandomFloat(-16.0f, 16.0f);
        piece.velocityY = RandomFloat(60.0f, 135.0f);
        piece.sway = RandomFloat(14.0f, 46.0f);
        piece.phase = RandomFloat(0.0f, 2.0f * kPi);
        piece.rotation = RandomFloat(0.0f, 2.0f * kPi);
        piece.rotationSpeed = RandomFloat(-5.0f, 5.0f);
        piece.width = RandomFloat(5.0f, 9.0f);
        piece.height = RandomFloat(9.0f, 17.0f);
        piece.lifetime = static_cast<float>(gHeight) / piece.velocityY + 1.0f;
        piece.red = color[0];
        piece.green = color[1];
        piece.blue = color[2];
        gConfetti.push_back(piece);
    }

    void SpawnBurst(const MonitorBounds& monitor, bool fromLeft, size_t count)
    {
        for (size_t index = 0; index < count; ++index)
        {
            const float angle = RandomFloat(0.78f, 1.42f);
            const float gravity = 300.0f;
            const float apexHeight = RandomFloat((monitor.bottom - monitor.top) * 0.35f,
                (monitor.bottom - monitor.top) * 1.05f);
            const float speed = std::sqrt(2.0f * gravity * apexHeight) / std::sin(angle);
            const float direction = fromLeft ? angle : kPi - angle;
            const auto& color = gPalette[RandomColor()];
            Confetti piece{};
            piece.x = fromLeft ? monitor.left + 14.0f : monitor.right - 14.0f;
            piece.y = monitor.bottom - 14.0f;
            piece.velocityX = std::cos(direction) * speed;
            piece.velocityY = -std::sin(direction) * speed;
            piece.rotation = RandomFloat(0.0f, 2.0f * kPi);
            piece.rotationSpeed = RandomFloat(-10.0f, 10.0f);
            piece.width = RandomFloat(5.0f, 10.0f);
            piece.height = RandomFloat(8.0f, 16.0f);
            piece.lifetime = (-2.0f * piece.velocityY / gravity) + RandomFloat(0.15f, 0.35f);
            piece.gravity = gravity;
            piece.red = color[0];
            piece.green = color[1];
            piece.blue = color[2];
            piece.burst = true;
            gConfetti.push_back(piece);
        }
    }

    void LaunchBursts()
    {
        const size_t launcherCount = gMonitors.size() * 2;
        if (launcherCount == 0 || gSettings.maxConfetti == 0)
        {
            return;
        }

        const size_t requestedPieces = static_cast<size_t>(gSettings.piecesPerCracker) * launcherCount;
        const size_t totalPieces = std::min(gSettings.maxConfetti, requestedPieces);
        size_t excessParticles = gConfetti.size() + totalPieces > gSettings.maxConfetti
            ? gConfetti.size() + totalPieces - gSettings.maxConfetti : 0;
        for (auto iterator = gConfetti.begin(); excessParticles > 0 && iterator != gConfetti.end();)
        {
            if (!iterator->burst)
            {
                iterator = gConfetti.erase(iterator);
                --excessParticles;
            }
            else
            {
                ++iterator;
            }
        }
        if (excessParticles > 0)
        {
            gConfetti.erase(gConfetti.begin(), gConfetti.begin() + excessParticles);
        }
        gConfetti.reserve(std::min(gSettings.maxConfetti, gConfetti.size() + totalPieces));

        const size_t piecesPerLauncher = totalPieces / launcherCount;
        const size_t extraPieces = totalPieces % launcherCount;
        const size_t firstLauncherWithExtra = gNextBurstStart % launcherCount;
        for (size_t launcher = 0; launcher < launcherCount; ++launcher)
        {
            const size_t offset = (launcher + launcherCount - firstLauncherWithExtra) % launcherCount;
            const size_t count = piecesPerLauncher + (offset < extraPieces ? 1 : 0);
            SpawnBurst(gMonitors[launcher / 2], launcher % 2 == 0, count);
        }
        gNextBurstStart = (firstLauncherWithExtra + 1) % launcherCount;
    }

    BYTE ApplyOpacity(BYTE alpha, BYTE opacity)
    {
        return static_cast<BYTE>(static_cast<unsigned int>(alpha) * opacity / 255u);
    }

    Point OffsetPoint(Point origin, Point direction, Point normal, float along, float across = 0.0f)
    {
        return {
            origin.x + direction.x * along + normal.x * across,
            origin.y + direction.y * along + normal.y * across
        };
    }

    void FillSegment(Point start, Point end, float thickness, BYTE red, BYTE green, BYTE blue, BYTE opacity)
    {
        const float dx = end.x - start.x;
        const float dy = end.y - start.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length <= 0.0f)
        {
            return;
        }
        const float nx = -dy / length * thickness * 0.5f;
        const float ny = dx / length * thickness * 0.5f;
        const Point quad[] = {
            { start.x + nx, start.y + ny }, { start.x - nx, start.y - ny },
            { end.x - nx, end.y - ny }, { end.x + nx, end.y + ny }
        };
        FillPolygon(quad, ARRAYSIZE(quad), red, green, blue, opacity);
    }

    void DrawPaperCone(Point base, Point direction, Point normal, float scale, BYTE opacity)
    {
        const float length = 94.0f * scale;
        const float rearRadius = 6.0f * scale;
        const float mouthRadius = 18.0f * scale;
        const Point mouth = OffsetPoint(base, direction, normal, length);
        const Point body[] = {
            OffsetPoint(base, direction, normal, 0.0f, rearRadius),
            OffsetPoint(base, direction, normal, 0.0f, -rearRadius),
            OffsetPoint(mouth, direction, normal, 0.0f, -mouthRadius),
            OffsetPoint(mouth, direction, normal, 0.0f, mouthRadius)
        };
        FillPolygon(body, ARRAYSIZE(body), 245, 215, 225, opacity);

        const BYTE stripeColors[][3] = {
            { 240, 55, 84 }, { 255, 202, 55 }, { 245, 75, 105 }, { 255, 202, 55 }
        };
        for (size_t stripe = 0; stripe < ARRAYSIZE(stripeColors); ++stripe)
        {
            const float start = 0.2f + static_cast<float>(stripe) * 0.19f;
            const float end = start + 0.095f;
            const float startWidth = rearRadius + (mouthRadius - rearRadius) * start;
            const float endWidth = rearRadius + (mouthRadius - rearRadius) * end;
            const Point band[] = {
                OffsetPoint(base, direction, normal, length * start, startWidth),
                OffsetPoint(base, direction, normal, length * start, -startWidth),
                OffsetPoint(base, direction, normal, length * end, -endWidth),
                OffsetPoint(base, direction, normal, length * end, endWidth)
            };
            FillPolygon(band, ARRAYSIZE(band), stripeColors[stripe][0], stripeColors[stripe][1],
                stripeColors[stripe][2], opacity);
        }

        FillEllipse(mouth, direction, normal, mouthRadius * 0.88f, mouthRadius * 0.38f,
            255, 208, 62, opacity);
        FillEllipse(mouth, direction, normal, mouthRadius * 0.61f, mouthRadius * 0.23f,
            105, 24, 48, opacity);
        FillEllipse(mouth, direction, normal, mouthRadius * 0.33f, mouthRadius * 0.11f,
            38, 12, 28, opacity);
    }

    void DrawFoilTube(Point base, Point direction, Point normal, float scale, BYTE opacity)
    {
        const float length = 83.0f * scale;
        const float radius = 13.0f * scale;
        const Point mouth = OffsetPoint(base, direction, normal, length);
        const Point shell[] = {
            OffsetPoint(base, direction, normal, 9.0f * scale, radius),
            OffsetPoint(base, direction, normal, 9.0f * scale, -radius),
            OffsetPoint(mouth, direction, normal, 0.0f, -radius),
            OffsetPoint(mouth, direction, normal, 0.0f, radius)
        };
        FillPolygon(shell, ARRAYSIZE(shell), 48, 139, 185, opacity);
        FillSegment(OffsetPoint(base, direction, normal, 15.0f * scale, radius * 0.48f),
            OffsetPoint(mouth, direction, normal, -3.0f * scale, radius * 0.48f),
            2.2f * scale, 184, 240, 248, opacity);

        const BYTE stripeColors[][3] = {
            { 255, 204, 78 }, { 248, 83, 112 }, { 255, 204, 78 }, { 248, 83, 112 }
        };
        for (size_t stripe = 0; stripe < ARRAYSIZE(stripeColors); ++stripe)
        {
            const float position = (0.24f + static_cast<float>(stripe) * 0.17f) * length;
            FillSegment(OffsetPoint(base, direction, normal, position, -radius * 0.85f),
                OffsetPoint(base, direction, normal, position, radius * 0.85f),
                4.5f * scale, stripeColors[stripe][0], stripeColors[stripe][1],
                stripeColors[stripe][2], opacity);
        }

        const Point back = OffsetPoint(base, direction, normal, 8.0f * scale);
        FillEllipse(back, direction, normal, radius * 0.9f, 5.0f * scale,
            207, 237, 243, opacity);
        FillEllipse(mouth, direction, normal, radius * 1.2f, 8.0f * scale,
            230, 244, 246, opacity);
        FillEllipse(mouth, direction, normal, radius * 0.87f, 5.5f * scale,
            24, 47, 67, opacity);

        Point cordStart = OffsetPoint(base, direction, normal, 4.0f * scale, -radius * 0.65f);
        Point cordMid = OffsetPoint(base, direction, normal, -7.0f * scale, -radius - 11.0f * scale);
        Point cordEnd{ cordMid.x - direction.x * 8.0f * scale, cordMid.y - direction.y * 8.0f * scale };
        FillSegment(cordStart, cordMid, 2.5f * scale, 255, 218, 137, opacity);
        FillSegment(cordMid, cordEnd, 2.5f * scale, 255, 218, 137, opacity);
        FillEllipse(cordEnd, direction, normal, 5.0f * scale, 6.0f * scale,
            255, 218, 137, opacity);
        FillEllipse(cordEnd, direction, normal, 2.8f * scale, 3.4f * scale,
            35, 44, 58, opacity);
    }

    void DrawPopSpring(Point base, Point direction, Point normal, float scale, BYTE opacity)
    {
        const float length = 96.0f * scale;
        const float rearRadius = 5.0f * scale;
        const float mouthRadius = 18.0f * scale;
        const Point mouth = OffsetPoint(base, direction, normal, length);
        const Point body[] = {
            OffsetPoint(base, direction, normal, 0.0f, rearRadius),
            OffsetPoint(base, direction, normal, 0.0f, -rearRadius),
            OffsetPoint(mouth, direction, normal, 0.0f, -mouthRadius),
            OffsetPoint(mouth, direction, normal, 0.0f, mouthRadius)
        };
        FillPolygon(body, ARRAYSIZE(body), 215, 191, 255, opacity);
        const BYTE colors[][3] = {
            { 255, 85, 127 }, { 255, 205, 82 }, { 69, 196, 255 },
            { 109, 224, 164 }, { 211, 117, 255 }
        };
        constexpr size_t folds = 9;
        for (size_t fold = 0; fold < folds; ++fold)
        {
            const float start = static_cast<float>(fold) / folds;
            const float end = static_cast<float>(fold + 1) / folds;
            const float middle = (start + end) * 0.5f;
            const float startRadius = rearRadius + (mouthRadius - rearRadius) * start;
            const float endRadius = rearRadius + (mouthRadius - rearRadius) * end;
            const float middleRadius = rearRadius + (mouthRadius - rearRadius) * middle;
            const Point ridge[] = {
                OffsetPoint(base, direction, normal, length * start, startRadius),
                OffsetPoint(base, direction, normal, length * middle, middleRadius * 0.68f),
                OffsetPoint(base, direction, normal, length * end, endRadius)
            };
            FillSegment(ridge[0], ridge[1], 4.0f * scale, colors[fold % ARRAYSIZE(colors)][0],
                colors[fold % ARRAYSIZE(colors)][1], colors[fold % ARRAYSIZE(colors)][2], opacity);
            FillSegment(OffsetPoint(base, direction, normal, length * start, -startRadius), ridge[1],
                4.0f * scale, colors[fold % ARRAYSIZE(colors)][0],
                colors[fold % ARRAYSIZE(colors)][1], colors[fold % ARRAYSIZE(colors)][2], opacity);
            FillSegment(ridge[1], OffsetPoint(base, direction, normal, length * end, -endRadius),
                4.0f * scale, colors[fold % ARRAYSIZE(colors)][0],
                colors[fold % ARRAYSIZE(colors)][1], colors[fold % ARRAYSIZE(colors)][2], opacity);
        }

        FillEllipse(mouth, direction, normal, mouthRadius, mouthRadius * 0.55f,
            255, 210, 82, opacity);
        FillEllipse(mouth, direction, normal, mouthRadius * 0.72f, mouthRadius * 0.38f,
            251, 109, 137, opacity);
        FillEllipse(mouth, direction, normal, mouthRadius * 0.43f, mouthRadius * 0.22f,
            46, 34, 79, opacity);
    }

    void DrawCracker(const MonitorBounds& monitor, bool atLeft, BYTE opacity, UINT style)
    {
        const float scale = std::clamp(std::min(monitor.right - monitor.left, monitor.bottom - monitor.top) / 900.0f,
            0.65f, 1.25f) * 1.12f;
        const Point direction = atLeft ? Point{ 0.6f, -0.8f } : Point{ -0.6f, -0.8f };
        const Point normal{ -direction.y, direction.x };
        const Point base{
            atLeft ? monitor.left + 11.0f * scale : monitor.right - 11.0f * scale,
            monitor.bottom - 4.0f * scale
        };
        switch (style)
        {
        case 2:
            DrawFoilTube(base, direction, normal, scale, opacity);
            break;
        case 3:
            DrawPopSpring(base, direction, normal, scale, opacity);
            break;
        default:
            DrawPaperCone(base, direction, normal, scale, opacity);
            break;
        }
    }

    void DrawCrackers(BYTE opacity, UINT style)
    {
        for (const MonitorBounds& monitor : gMonitors)
        {
            DrawCracker(monitor, true, opacity, style);
            DrawCracker(monitor, false, opacity, style);
        }
    }

    void Render(float deltaSeconds)
    {
        if (!gPixels || !gWindow)
        {
            return;
        }

        std::fill(gPixels, gPixels + static_cast<size_t>(gWidth) * gHeight * 4, static_cast<BYTE>(0));
        const ULONGLONG now = GetTickCount64();
        if (now - gLastSpawn >= gSettings.rainIntervalMs)
        {
            const ULONGLONG intervals = std::min<ULONGLONG>(
                (now - gLastSpawn) / gSettings.rainIntervalMs, 20);
            gLastSpawn += intervals * gSettings.rainIntervalMs;
            for (ULONGLONG index = 0; index < intervals; ++index)
            {
                SpawnFallingConfetti();
            }
        }

        for (Confetti& piece : gConfetti)
        {
            const float previousAge = piece.age;
            piece.age += deltaSeconds;
            if (piece.burst)
            {
                piece.velocityY += piece.gravity * deltaSeconds;
                piece.x += piece.velocityX * deltaSeconds;
                piece.y += piece.velocityY * deltaSeconds;
            }
            else
            {
                constexpr float frequency = 2.8f;
                const float previousWave = std::sin((previousAge + piece.phase) * frequency);
                const float currentWave = std::sin((piece.age + piece.phase) * frequency);
                piece.x += piece.velocityX * deltaSeconds +
                    (currentWave - previousWave) * piece.sway / frequency;
                piece.y += piece.velocityY * deltaSeconds;
            }
            piece.rotation += piece.rotationSpeed * deltaSeconds;

            if (piece.age < piece.lifetime && piece.y < static_cast<float>(gHeight) + 30.0f &&
                piece.x > -30.0f && piece.x < static_cast<float>(gWidth) + 30.0f)
            {
                const float opacity = piece.burst
                    ? std::clamp((piece.lifetime - piece.age) / 0.25f, 0.0f, 1.0f)
                    : 1.0f;
                DrawConfetti(piece, static_cast<BYTE>(220.0f * opacity));
            }
        }
        gConfetti.erase(std::remove_if(gConfetti.begin(), gConfetti.end(),
            [](const Confetti& piece)
            {
                return piece.age >= piece.lifetime || piece.y >= static_cast<float>(gHeight) + 30.0f ||
                    piece.x <= -30.0f || piece.x >= static_cast<float>(gWidth) + 30.0f;
            }), gConfetti.end());
        if (now < gCrackerVisibleUntil)
        {
            constexpr ULONGLONG fadeDurationMs = 350;
            const ULONGLONG timeRemaining = gCrackerVisibleUntil - now;
            const BYTE opacity = static_cast<BYTE>(std::min<ULONGLONG>(255,
                timeRemaining * 255 / fadeDurationMs));
            DrawCrackers(opacity, gCurrentCrackerStyle);
        }

        HDC screenDc = GetDC(nullptr);
        if (!screenDc)
        {
            MessageBoxW(gWindow, L"Could not access the display for drawing.", kAppTitle, MB_OK | MB_ICONERROR);
            PostMessageW(gWindow, WM_CLOSE, 0, 0);
            return;
        }
        POINT destination{ gScreenX, gScreenY };
        POINT source{ 0, 0 };
        SIZE size{ gWidth, gHeight };
        BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        const BOOL presented = UpdateLayeredWindow(
            gWindow, screenDc, &destination, &size, gMemoryDc, &source, 0, &blend, ULW_ALPHA);
        ReleaseDC(nullptr, screenDc);
        if (!presented)
        {
            MessageBoxW(gWindow, L"Could not update the confetti overlay.", kAppTitle, MB_OK | MB_ICONERROR);
            PostMessageW(gWindow, WM_CLOSE, 0, 0);
        }
    }

    void Cleanup()
    {
        if (gKeyboardHook)
        {
            UnhookWindowsHookEx(gKeyboardHook);
            gKeyboardHook = nullptr;
        }
        if (gMouseHook)
        {
            UnhookWindowsHookEx(gMouseHook);
            gMouseHook = nullptr;
        }
        if (gMemoryDc && gPreviousBitmap)
        {
            SelectObject(gMemoryDc, gPreviousBitmap);
            gPreviousBitmap = nullptr;
        }
        if (gBitmap)
        {
            DeleteObject(gBitmap);
            gBitmap = nullptr;
            gPixels = nullptr;
        }
        if (gMemoryDc)
        {
            DeleteDC(gMemoryDc);
            gMemoryDc = nullptr;
        }
    }

    void QueueConfetti()
    {
        const ULONGLONG now = GetTickCount64();
        if (gWindow && now - gLastInput >= kInputCooldownMs)
        {
            gLastInput = now;
            PostMessageW(gWindow, kConfettiMessage, 0, 0);
        }
    }

    void TriggerConfettiBurst()
    {
        gCrackerVisibleUntil = GetTickCount64() + gSettings.crackerVisibleMs;
        LaunchBursts();
    }

    LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code == HC_ACTION)
        {
            const auto* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
            if (key->vkCode < ARRAYSIZE(gPressedKeys))
            {
                if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
                {
                    gPressedKeys[key->vkCode] = false;
                }
                else if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
                {
                    const bool wasPressed = gPressedKeys[key->vkCode];
                    gPressedKeys[key->vkCode] = true;
                    if (!wasPressed)
                    {
                        if (key->vkCode == VK_ESCAPE && gWindow)
                        {
                            PostMessageW(gWindow, WM_CLOSE, 0, 0);
                        }
                        else if (key->vkCode >= VK_F1 && key->vkCode <= VK_F3 && gWindow)
                        {
                            PostMessageW(gWindow, kSelectCrackerStyleMessage,
                                key->vkCode - VK_F1 + 1, 0);
                        }
                        else
                        {
                            QueueConfetti();
                        }
                    }
                }
            }
        }
        return CallNextHookEx(gKeyboardHook, code, wParam, lParam);
    }

    LRESULT CALLBACK MouseHookProc(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code == HC_ACTION &&
            (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN))
        {
            QueueConfetti();
        }
        return CallNextHookEx(gMouseHook, code, wParam, lParam);
    }

    LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case kConfettiMessage:
            TriggerConfettiBurst();
            return 0;
        case kSelectCrackerStyleMessage:
            gCurrentCrackerStyle = static_cast<UINT>(wParam);
            TriggerConfettiBurst();
            return 0;
        case WM_TIMER:
            if (wParam == kFrameTimerId && gRunning)
            {
                const ULONGLONG now = GetTickCount64();
                const float elapsed = std::clamp(static_cast<float>(now - gLastFrame) / 1000.0f, 0.0f, 0.1f);
                gLastFrame = now;
                Render(elapsed);
            }
            return 0;
        case WM_DISPLAYCHANGE:
            gRunning = false;
            PostMessageW(window, WM_CLOSE, 0, 0);
            return 0;
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            gRunning = false;
            KillTimer(window, kFrameTimerId);
            Cleanup();
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, wParam, lParam);
        }
    }

    bool InitializeOverlay()
    {
        gScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
        gScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
        gWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        gHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (gWidth <= 0 || gHeight <= 0)
        {
            return false;
        }
        gMonitors.clear();
        if (!EnumDisplayMonitors(nullptr, nullptr,
            [](HMONITOR, HDC, LPRECT bounds, LPARAM)
            {
                gMonitors.push_back({
                    static_cast<float>(bounds->left - gScreenX),
                    static_cast<float>(bounds->top - gScreenY),
                    static_cast<float>(bounds->right - gScreenX),
                    static_cast<float>(bounds->bottom - gScreenY)
                });
                return TRUE;
            }, 0) || gMonitors.empty())
        {
            return false;
        }

        gMemoryDc = CreateCompatibleDC(nullptr);
        if (!gMemoryDc)
        {
            return false;
        }

        BITMAPINFO bitmapInfo{};
        bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth = gWidth;
        bitmapInfo.bmiHeader.biHeight = -gHeight;
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 32;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;
        gBitmap = CreateDIBSection(gMemoryDc, &bitmapInfo, DIB_RGB_COLORS,
            reinterpret_cast<void**>(&gPixels), nullptr, 0);
        if (!gBitmap || !gPixels)
        {
            return false;
        }
        gPreviousBitmap = static_cast<HBITMAP>(SelectObject(gMemoryDc, gBitmap));
        return gPreviousBitmap != nullptr && gPreviousBitmap != HGDI_ERROR;
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    gInstance = instance;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&windowClass))
    {
        MessageBoxW(nullptr, L"Could not register the overlay window.", kAppTitle, MB_OK | MB_ICONERROR);
        return 1;
    }
    gSettingsPath = GetExecutableDirectory();
    if (gSettingsPath.empty())
    {
        MessageBoxW(nullptr, L"Could not locate the application folder.", kAppTitle, MB_OK | MB_ICONERROR);
        return 1;
    }
    gSettingsPath += L"\\";
    gSettingsPath += kSettingsFileName;
    if (!EnsureSettingsFile())
    {
        MessageBoxW(nullptr, L"Could not create or access settings.ini.", kAppTitle, MB_OK | MB_ICONERROR);
        return 1;
    }
    LoadSettings();

    gWindow = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        kWindowClass, kAppTitle, WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (!gWindow)
    {
        MessageBoxW(nullptr, L"Could not create the overlay window.", kAppTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!InitializeOverlay())
    {
        Cleanup();
        DestroyWindow(gWindow);
        MessageBoxW(nullptr, L"Could not initialize the confetti canvas.", kAppTitle, MB_OK | MB_ICONERROR);
        return 1;
    }
    if (!SetWindowDisplayAffinity(gWindow, WDA_EXCLUDEFROMCAPTURE))
    {
        Cleanup();
        DestroyWindow(gWindow);
        MessageBoxW(nullptr, L"Could not exclude the overlay from screen capture.", kAppTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    gKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, instance, 0);
    if (!gKeyboardHook)
    {
        Cleanup();
        DestroyWindow(gWindow);
        MessageBoxW(nullptr, L"Could not install the keyboard hook.", kAppTitle, MB_OK | MB_ICONERROR);
        return 1;
    }
    gMouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, instance, 0);
    if (!gMouseHook)
    {
        Cleanup();
        DestroyWindow(gWindow);
        MessageBoxW(nullptr, L"Could not install the mouse hook.", kAppTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!SetTimer(gWindow, kFrameTimerId, 16, nullptr))
    {
        Cleanup();
        DestroyWindow(gWindow);
        MessageBoxW(nullptr, L"Could not start the animation timer.", kAppTitle, MB_OK | MB_ICONERROR);
        return 1;
    }
    gLastFrame = GetTickCount64();
    gLastSpawn = gLastFrame;
    ShowWindow(gWindow, SW_SHOWNOACTIVATE);
    Render(0.016f);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
