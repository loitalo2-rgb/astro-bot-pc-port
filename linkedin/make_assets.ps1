Add-Type -AssemblyName System.Drawing

$cs = @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Drawing.Text;

public class LI
{
    static Font F(string name, float size, FontStyle st) { return new Font(name, size, st, GraphicsUnit.Pixel); }

    public static void CoverDark(string path, int w, int h)
    {
        using (var bmp = new Bitmap(w, h))
        using (var g = Graphics.FromImage(bmp))
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;

            var c1 = ColorTranslator.FromHtml("#07131F");
            var c2 = ColorTranslator.FromHtml("#0E3A5C");
            using (var br = new LinearGradientBrush(new Rectangle(0, 0, w, h), c1, c2, 15f))
                g.FillRectangle(br, 0, 0, w, h);

            // soft glow top right
            using (var pth = new GraphicsPath())
            {
                pth.AddEllipse(w - 520, -260, 900, 700);
                using (var pg = new PathGradientBrush(pth))
                {
                    pg.CenterColor = Color.FromArgb(70, 41, 150, 235);
                    pg.SurroundColors = new Color[] { Color.FromArgb(0, 41, 150, 235) };
                    g.FillPath(pg, pth);
                }
            }

            // concentric rings
            float cx = w - 170f, cy = h / 2f;
            for (int i = 0; i < 6; i++)
            {
                float r = 58 + i * 46;
                using (var p = new Pen(Color.FromArgb(26 + i * 3, 74, 178, 255), 1.4f))
                    g.DrawEllipse(p, cx - r, cy - r, r * 2, r * 2);
            }
            using (var p2 = new Pen(Color.FromArgb(120, 74, 178, 255), 3f))
                g.DrawArc(p2, cx - 150, cy - 150, 300, 300, 200, 55);

            // dot grid
            for (int x = 0; x < w; x += 26)
                for (int y = 0; y < h; y += 26)
                {
                    int a = 14;
                    if (x > w - 640) a = 26;
                    using (var db = new SolidBrush(Color.FromArgb(a, 150, 205, 255)))
                        g.FillEllipse(db, x, y, 2, 2);
                }

            int left = 232;

            // accent bar
            using (var ab = new SolidBrush(ColorTranslator.FromHtml("#2E9BFF")))
                g.FillRectangle(ab, left, 96, 5, 196);

            // name
            using (var f = F("Segoe UI", 46, FontStyle.Bold))
            using (var b = new SolidBrush(Color.White))
                g.DrawString("\u00cdTALO ALVES", f, b, left + 30, 88);

            // role
            using (var f = F("Segoe UI", 21, FontStyle.Regular))
            using (var b = new SolidBrush(ColorTranslator.FromHtml("#BBD9F7")))
                g.DrawString("Suporte T\u00e9cnico e Infraestrutura", f, b, left + 32, 160);

            // keywords
            using (var f = F("Segoe UI", 15, FontStyle.Regular))
            using (var b = new SolidBrush(ColorTranslator.FromHtml("#7FB0D8")))
                g.DrawString("Help Desk N1/N2   \u00b7   Windows   \u00b7   Redes   \u00b7   Hardware   \u00b7   Pacote Office", f, b, left + 32, 200);

            // education / location
            using (var f = F("Segoe UI", 14, FontStyle.Regular))
            using (var b = new SolidBrush(ColorTranslator.FromHtml("#5E88AC")))
                g.DrawString("An\u00e1lise e Desenvolvimento de Sistemas   \u00b7   Sorocaba, SP", f, b, left + 32, 232);

            // url chip
            string url = "linkedin.com/in/italo-alves-ti";
            using (var f = F("Segoe UI", 14, FontStyle.Bold))
            {
                var sz = g.MeasureString(url, f);
                var rect = new RectangleF(left + 32, 272, sz.Width + 28, 32);
                using (var pen = new Pen(Color.FromArgb(90, 74, 178, 255), 1.2f))
                    g.DrawRectangle(pen, rect.X, rect.Y, rect.Width, rect.Height);
                using (var b = new SolidBrush(ColorTranslator.FromHtml("#9CCBEE")))
                    g.DrawString(url, f, b, rect.X + 14, rect.Y + 7);
            }

            bmp.Save(path, ImageFormat.Png);
        }
    }

    public static void CoverLight(string path, int w, int h)
    {
        using (var bmp = new Bitmap(w, h))
        using (var g = Graphics.FromImage(bmp))
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;

            using (var b = new SolidBrush(ColorTranslator.FromHtml("#0B1B2B")))
                g.FillRectangle(b, 0, 0, w, h);

            // diagonal band: dark panel occupies the left 66%
            using (var b = new SolidBrush(ColorTranslator.FromHtml("#F2F6FA")))
            {
                var pts = new PointF[] { new PointF(w * 0.70f, 0), new PointF(w, 0), new PointF(w, h), new PointF(w * 0.62f, h) };
                g.FillPolygon(b, pts);
            }
            using (var b = new SolidBrush(ColorTranslator.FromHtml("#2E9BFF")))
            {
                var pts = new PointF[] { new PointF(w * 0.678f, 0), new PointF(w * 0.70f, 0), new PointF(w * 0.62f, h), new PointF(w * 0.598f, h) };
                g.FillPolygon(b, pts);
            }

            int left = 232;

            using (var f = F("Segoe UI", 46, FontStyle.Bold))
            using (var b = new SolidBrush(Color.White))
                g.DrawString("\u00cdTALO ALVES", f, b, left, 96);

            using (var f = F("Segoe UI", 21, FontStyle.Regular))
            using (var b = new SolidBrush(ColorTranslator.FromHtml("#8FC4F0")))
                g.DrawString("Suporte T\u00e9cnico  |  Help Desk N1/N2", f, b, left, 170);

            using (var f = F("Segoe UI", 15, FontStyle.Regular))
            using (var b = new SolidBrush(ColorTranslator.FromHtml("#6E96BA")))
                g.DrawString("Windows  \u00b7  Redes  \u00b7  Hardware  \u00b7  Pacote Office", f, b, left, 212);

            using (var f = F("Segoe UI", 14, FontStyle.Regular))
            using (var b = new SolidBrush(ColorTranslator.FromHtml("#4E759A")))
                g.DrawString("ADS  \u00b7  Sorocaba, SP  \u00b7  linkedin.com/in/italo-alves-ti", f, b, left, 244);

            // right side marks on the light band
            using (var pen = new Pen(Color.FromArgb(55, 46, 155, 255), 1.2f))
                for (int i = 0; i < 8; i++)
                    g.DrawLine(pen, w - 330 + i * 20, h * 0.28f, w - 330 + i * 20, h * 0.68f);

            using (var f = F("Segoe UI", 14, FontStyle.Bold))
            using (var b = new SolidBrush(ColorTranslator.FromHtml("#8AA3B8")))
                g.DrawString("TI  \u00b7  INFRAESTRUTURA", f, b, w - 330, h * 0.72f);

            bmp.Save(path, ImageFormat.Png);
        }
    }

    public static Bitmap RetouchPhoto(string path)
    {
        var src = new Bitmap(path);
        int side = Math.Min(src.Width, src.Height);
        // crop 1.08x zoom toward face, slightly above center
        int cw = (int)(side / 1.08);
        int cx = src.Width / 2;
        int cy = (int)(src.Height * 0.44);
        int x0 = Math.Max(0, Math.Min(src.Width - cw, cx - cw / 2));
        int y0 = Math.Max(0, Math.Min(src.Height - cw, cy - cw / 2));
        var crop = src.Clone(new Rectangle(x0, y0, cw, cw), PixelFormat.Format32bppArgb);
        var outp = new Bitmap(800, 800);
        using (var g = Graphics.FromImage(outp))
        {
            g.InterpolationMode = InterpolationMode.HighQualityBicubic;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
            g.DrawImage(crop, 0, 0, 800, 800);
        }
        crop.Dispose(); src.Dispose();

        Adjust(outp, 0.010f, 1.10f, 0.80f, 0.16f, 1.02f, 1.0f);
        PortraitBlur(outp, 5.5f, 400, 300, 250, 520);
        Vignette(outp, 0.26f);
        return outp;
    }

    public static Bitmap StudioPhoto(string path)
    {
        var src = new Bitmap(path);
        int side = Math.Min(src.Width, src.Height);
        int cw = (int)(side / 1.05);
        int cx = src.Width / 2;
        int cy = (int)(src.Height * 0.46);
        int x0 = Math.Max(0, Math.Min(src.Width - cw, cx - cw / 2));
        int y0 = Math.Max(0, Math.Min(src.Height - cw, cy - cw / 2));
        var crop = src.Clone(new Rectangle(x0, y0, cw, cw), PixelFormat.Format32bppArgb);
        var outp = new Bitmap(800, 800);
        using (var g = Graphics.FromImage(outp))
        {
            g.InterpolationMode = InterpolationMode.HighQualityBicubic;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
            g.DrawImage(crop, 0, 0, 800, 800);
        }
        crop.Dispose(); src.Dispose();

        Adjust(outp, 0.008f, 1.12f, 0.82f, 0.14f, 1.03f, 1.0f);

        // background replacement layer: dark, desaturated, blurred
        using (var bg = new Bitmap(800, 800))
        {
            using (var g = Graphics.FromImage(bg))
            {
                g.InterpolationMode = InterpolationMode.HighQualityBilinear;
                using (var small = new Bitmap(100, 100))
                using (var sg = Graphics.FromImage(small))
                {
                    sg.InterpolationMode = InterpolationMode.HighQualityBilinear;
                    sg.DrawImage(outp, 0, 0, 100, 100);
                    g.DrawImage(small, 0, 0, 800, 800);
                }
                using (var ov = new SolidBrush(Color.FromArgb(120, 8, 20, 32)))
                    g.FillRectangle(ov, 0, 0, 800, 800);
            }
            Adjust(bg, -0.02f, 0.86f, 0.9f, 0.0f, 0.45f, 1.0f);
            EllipseComposite(outp, bg, 400, 330, 268, 372, 120);
        }

        Vignette(outp, 0.34f);
        return outp;
    }

    static void EllipseComposite(Bitmap bmp, Bitmap bg, float cx, float cy, float rx, float ry, float feather)
    {
        int w = bmp.Width, h = bmp.Height;
        var rect = new Rectangle(0, 0, w, h);
        var d1 = bmp.LockBits(rect, ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
        var d2 = bg.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        int stride = d1.Stride; int len = Math.Abs(stride) * h;
        byte[] a = new byte[len]; byte[] b = new byte[len];
        System.Runtime.InteropServices.Marshal.Copy(d1.Scan0, a, 0, len);
        System.Runtime.InteropServices.Marshal.Copy(d2.Scan0, b, 0, len);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                float nx = (x - cx) / rx, ny = (y - cy) / ry;
                float dist = (float)Math.Sqrt(nx * nx + ny * ny);
                float edge = 1f + feather / rx;
                float t = (dist - 1f) / (edge - 1f);
                t = t < 0 ? 0 : (t > 1 ? 1 : t);
                t = t * t * (3 - 2 * t);
                if (t <= 0) continue;
                int i = y * stride + x * 4;
                a[i] = (byte)(a[i] * (1 - t) + b[i] * t);
                a[i + 1] = (byte)(a[i + 1] * (1 - t) + b[i + 1] * t);
                a[i + 2] = (byte)(a[i + 2] * (1 - t) + b[i + 2] * t);
            }
        System.Runtime.InteropServices.Marshal.Copy(a, 0, d1.Scan0, len);
        bmp.UnlockBits(d1); bg.UnlockBits(d2);
    }

    static void Adjust(Bitmap bmp, float bright, float contrast, float hiRoll, float shadowLift, float sat, float gamma)
    {
        var rect = new Rectangle(0, 0, bmp.Width, bmp.Height);
        var data = bmp.LockBits(rect, ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
        int len = Math.Abs(data.Stride) * bmp.Height;
        byte[] buf = new byte[len];
        System.Runtime.InteropServices.Marshal.Copy(data.Scan0, buf, 0, len);

        for (int i = 0; i < len; i += 4)
        {
            float b = buf[i] / 255f, g = buf[i + 1] / 255f, r = buf[i + 2] / 255f;
            r = (float)Math.Pow(r, gamma); g = (float)Math.Pow(g, gamma); b = (float)Math.Pow(b, gamma);
            r = (r - 0.5f) * contrast + 0.5f; g = (g - 0.5f) * contrast + 0.5f; b = (b - 0.5f) * contrast + 0.5f;
            r += bright; g += bright; b += bright;
            if (r > 0.72f) r = 0.72f + (r - 0.72f) * hiRoll;
            if (g > 0.72f) g = 0.72f + (g - 0.72f) * hiRoll;
            if (b > 0.72f) b = 0.72f + (b - 0.72f) * hiRoll;
            if (r < 0.30f) r = r + (0.30f - r) * shadowLift;
            if (g < 0.30f) g = g + (0.30f - g) * shadowLift;
            if (b < 0.30f) b = b + (0.30f - b) * shadowLift;
            float lum = 0.299f * r + 0.587f * g + 0.114f * b;
            r = lum + (r - lum) * sat; g = lum + (g - lum) * sat; b = lum + (b - lum) * sat;
            buf[i] = (byte)Math.Max(0, Math.Min(255, b * 255f));
            buf[i + 1] = (byte)Math.Max(0, Math.Min(255, g * 255f));
            buf[i + 2] = (byte)Math.Max(0, Math.Min(255, r * 255f));
        }
        System.Runtime.InteropServices.Marshal.Copy(buf, 0, data.Scan0, len);
        bmp.UnlockBits(data);
    }

    static void PortraitBlur(Bitmap bmp, float radius, float fx, float fy, float inner, float outer)
    {
        int w = bmp.Width, h = bmp.Height;
        using (var blurred = new Bitmap(w, h))
        {
            using (var g = Graphics.FromImage(blurred))
            {
                g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                // cheap blur: downscale/upscale
                using (var small = new Bitmap(w / 8, h / 8))
                using (var sg = Graphics.FromImage(small))
                {
                    sg.InterpolationMode = InterpolationMode.HighQualityBilinear;
                    sg.DrawImage(bmp, 0, 0, small.Width, small.Height);
                    g.DrawImage(small, 0, 0, w, h);
                }
            }

            var rect = new Rectangle(0, 0, w, h);
            var d1 = bmp.LockBits(rect, ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
            var d2 = blurred.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            int stride = d1.Stride;
            int len = Math.Abs(stride) * h;
            byte[] a = new byte[len]; byte[] b = new byte[len];
            System.Runtime.InteropServices.Marshal.Copy(d1.Scan0, a, 0, len);
            System.Runtime.InteropServices.Marshal.Copy(d2.Scan0, b, 0, len);

            for (int y = 0; y < h; y++)
            {
                for (int x = 0; x < w; x++)
                {
                    float dx = x - fx, dy = y - fy;
                    float dist = (float)Math.Sqrt(dx * dx + dy * dy);
                    float t = (dist - inner) / (outer - inner);
                    t = t < 0 ? 0 : (t > 1 ? 1 : t);
                    t = t * t * (3 - 2 * t);
                    if (t <= 0) continue;
                    int i = y * stride + x * 4;
                    a[i] = (byte)(a[i] * (1 - t) + b[i] * t);
                    a[i + 1] = (byte)(a[i + 1] * (1 - t) + b[i + 1] * t);
                    a[i + 2] = (byte)(a[i + 2] * (1 - t) + b[i + 2] * t);
                }
            }
            System.Runtime.InteropServices.Marshal.Copy(a, 0, d1.Scan0, len);
            bmp.UnlockBits(d1); blurred.UnlockBits(d2);
        }
    }

    static void Vignette(Bitmap bmp, float strength)
    {
        int w = bmp.Width, h = bmp.Height;
        var rect = new Rectangle(0, 0, w, h);
        var d = bmp.LockBits(rect, ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
        int stride = d.Stride; int len = Math.Abs(stride) * h;
        byte[] a = new byte[len];
        System.Runtime.InteropServices.Marshal.Copy(d.Scan0, a, 0, len);
        float cx = w / 2f, cy = h / 2f;
        float maxd = (float)Math.Sqrt(cx * cx + cy * cy);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                float dx = x - cx, dy = y - cy;
                float dist = (float)Math.Sqrt(dx * dx + dy * dy) / maxd;
                float f = dist * dist * strength;
                int i = y * stride + x * 4;
                a[i] = (byte)(a[i] * (1 - f));
                a[i + 1] = (byte)(a[i + 1] * (1 - f));
                a[i + 2] = (byte)(a[i + 2] * (1 - f));
            }
        System.Runtime.InteropServices.Marshal.Copy(a, 0, d.Scan0, len);
        bmp.UnlockBits(d);
    }
}
'@

Add-Type -TypeDefinition $cs -ReferencedAssemblies System.Drawing

$dir = Split-Path -Parent $MyInvocation.MyCommand.Path
[LI]::CoverDark((Join-Path $dir "capa_opcao1.png"), 1584, 396)
[LI]::CoverLight((Join-Path $dir "capa_opcao2.png"), 1584, 396)

$photo = [LI]::RetouchPhoto((Join-Path $dir "foto_800.jpg"))
$enc = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$ps = New-Object System.Drawing.Imaging.EncoderParameters(1)
$ps.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::Quality, 95L)
$photo.Save((Join-Path $dir "foto_perfil_nova.jpg"), $enc, $ps)
$photo.Dispose()

$photo2 = [LI]::StudioPhoto((Join-Path $dir "foto_800.jpg"))
$photo2.Save((Join-Path $dir "foto_perfil_estudio.jpg"), $enc, $ps)
$photo2.Dispose()

Write-Output "OK"
Get-ChildItem $dir -Filter *.png | Select-Object Name, Length
Get-ChildItem $dir -Filter foto_perfil_nova.jpg | Select-Object Name, Length
