import os
import sys
import subprocess
import configparser
import tkinter as tk
from tkinter import ttk, messagebox

CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "astro_config.ini")
HYBRID_DIR = r"D:\Jogos ps4\AstroHybrid\AstroBotPC-AstroHybrid-1.0.9"
RUN_HYBRID_DIR = os.path.join(HYBRID_DIR, "Build", "RunHybrid")
PDF_REPORT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Relatorio_Completo_AstroBot_PC.pdf")

def load_config():
    config = configparser.ConfigParser()
    if os.path.exists(CONFIG_PATH):
        config.read(CONFIG_PATH, encoding="utf-8")
    else:
        config["Graphics"] = {"width": "2560", "height": "1440", "fullscreen": "1", "vsync": "1", "fps_limit": "60", "perf_display": "0"}
        config["Controls"] = {"pad_type": "dualsense", "trigger_state": "1", "pad_motion": "1", "rumble_scale": "100"}
        config["Engine_GPU"] = {"shader_cache": "1", "cache_title_id": "PPSA21567", "pipeline_prefetch": "1", "pipeline_prewarm": "1", "rt_software": "auto", "buffer_range_fixes": "1", "commit_epoch_fastpath": "0"}
        config["CPU"] = {"cp_priority": "2", "draw_prep_priority": "2"}
        save_config(config)
    return config

def save_config(config):
    with open(CONFIG_PATH, "w", encoding="utf-8") as f:
        config.write(f)

class AstroLauncherApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Astro Bot PC - Launcher & Painel de Controle (By: @loitalo2-rgb)")
        self.geometry("640x540")
        self.resizable(False, False)
        self.config = load_config()
        self.configure(bg="#121826")
        
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure("TLabel", background="#121826", foreground="#E2E8F0", font=("Segoe UI", 10))
        style.configure("Header.TLabel", background="#121826", foreground="#38BDF8", font=("Segoe UI", 16, "bold"))
        style.configure("SubHeader.TLabel", background="#121826", foreground="#94A3B8", font=("Segoe UI", 9))
        style.configure("TCheckbutton", background="#121826", foreground="#E2E8F0", font=("Segoe UI", 10))
        style.configure("TCombobox", fieldbackground="#1E293B", background="#334155", foreground="#F8FAFC", font=("Segoe UI", 10))
        
        self.create_widgets()

    def create_widgets(self):
        # Header
        header = ttk.Label(self, text="ASTRO BOT PC (Port Nativo PS5)", style="Header.TLabel")
        header.pack(pady=(15, 2))
        sub = ttk.Label(self, text="Otimizações de Gráficos, Controles DualSense/Xbox & Shader Cache", style="SubHeader.TLabel")
        sub.pack(pady=(0, 15))

        main_frame = tk.Frame(self, bg="#1E293B", padx=20, pady=15, relief="ridge", bd=1)
        main_frame.pack(fill="both", expand=True, padx=20, pady=(0, 15))

        # 1. Resolução
        lbl_res = ttk.Label(main_frame, text="Resolução Gráfica:")
        lbl_res.grid(row=0, column=0, sticky="w", pady=8)
        self.res_var = tk.StringVar()
        w = self.config.get("Graphics", "width", fallback="2560")
        h = self.config.get("Graphics", "height", fallback="1440")
        cur_res = f"{w}x{h}"
        self.res_cb = ttk.Combobox(main_frame, textvariable=self.res_var, state="readonly", width=25)
        self.res_cb["values"] = ("1920x1080 (Full HD)", "2560x1440 (2K Quad HD - Recomendado)", "3840x2160 (4K Ultra HD)")
        if "3840" in w:
            self.res_cb.current(2)
        elif "2560" in w:
            self.res_cb.current(1)
        else:
            self.res_cb.current(0)
        self.res_cb.grid(row=0, column=1, sticky="e", pady=8)

        # 2. Taxa de Quadros / VSync
        lbl_fps = ttk.Label(main_frame, text="Taxa de Quadros:")
        lbl_fps.grid(row=1, column=0, sticky="w", pady=8)
        self.fps_var = tk.StringVar()
        self.fps_cb = ttk.Combobox(main_frame, textvariable=self.fps_var, state="readonly", width=25)
        self.fps_cb["values"] = ("60 FPS (VSync Estável)", "120 FPS (Alta Fluidez)", "Desbloqueado (FreeSync/G-Sync)")
        fps_cfg = self.config.get("Graphics", "fps_limit", fallback="60")
        if fps_cfg == "120":
            self.fps_cb.current(1)
        elif fps_cfg == "0":
            self.fps_cb.current(2)
        else:
            self.fps_cb.current(0)
        self.fps_cb.grid(row=1, column=1, sticky="e", pady=8)

        # 3. Tipo de Controle
        lbl_pad = ttk.Label(main_frame, text="Perfil de Controle:")
        lbl_pad.grid(row=2, column=0, sticky="w", pady=8)
        self.pad_var = tk.StringVar()
        self.pad_cb = ttk.Combobox(main_frame, textvariable=self.pad_var, state="readonly", width=25)
        self.pad_cb["values"] = ("DualSense PS5 (Hápticos + Gatilhos L2/R2)", "Controle Xbox / XInput", "Teclado e Mouse")
        pad_cfg = self.config.get("Controls", "pad_type", fallback="dualsense")
        if pad_cfg == "xbox":
            self.pad_cb.current(1)
        elif pad_cfg == "keyboard":
            self.pad_cb.current(2)
        else:
            self.pad_cb.current(0)
        self.pad_cb.grid(row=2, column=1, sticky="e", pady=8)

        # 4. Checkbox Gatilhos Adaptativos e Sensores de Movimento
        self.motion_var = tk.BooleanVar(value=(self.config.get("Controls", "pad_motion", fallback="1") == "1"))
        chk_motion = tk.Checkbutton(main_frame, text="Ativar Giroscópio / Sensor de Movimento (Shake & Tilt)",
                                    variable=self.motion_var, bg="#1E293B", fg="#E2E8F0", selectcolor="#0F172A",
                                    activebackground="#1E293B", activeforeground="#38BDF8")
        chk_motion.grid(row=3, column=0, columnspan=2, sticky="w", pady=6)

        # 5. Checkbox Cache de Shaders Antecipado (Zero Stutter)
        self.cache_var = tk.BooleanVar(value=(self.config.get("Engine_GPU", "shader_cache", fallback="1") == "1"))
        chk_cache = tk.Checkbutton(main_frame, text="Usar Shader & Pipeline Cache Vulkan (Elimina micro-travadas)",
                                   variable=self.cache_var, bg="#1E293B", fg="#E2E8F0", selectcolor="#0F172A",
                                   activebackground="#1E293B", activeforeground="#38BDF8")
        chk_cache.grid(row=4, column=0, columnspan=2, sticky="w", pady=6)

        # 6. Checkbox HUD de Desempenho (FPS na tela)
        self.perf_var = tk.BooleanVar(value=(self.config.get("Graphics", "perf_display", fallback="0") == "1"))
        chk_perf = tk.Checkbutton(main_frame, text="Exibir HUD de Desempenho Nativo (FPS, CPU/GPU, VRAM)",
                                  variable=self.perf_var, bg="#1E293B", fg="#E2E8F0", selectcolor="#0F172A",
                                  activebackground="#1E293B", activeforeground="#38BDF8")
        chk_perf.grid(row=5, column=0, columnspan=2, sticky="w", pady=6)

        # Botões inferiores
        btn_frame = tk.Frame(self, bg="#121826")
        btn_frame.pack(fill="x", padx=20, pady=(0, 15))

        btn_launch = tk.Button(btn_frame, text="🚀 INICIAR ASTRO BOT PC", font=("Segoe UI", 12, "bold"),
                               bg="#0284C7", fg="#FFFFFF", activebackground="#0369A1", activeforeground="#FFFFFF",
                               padx=15, pady=8, bd=0, cursor="hand2", command=self.launch_game)
        btn_launch.pack(fill="x", pady=(0, 8))

        sub_btn_frame = tk.Frame(btn_frame, bg="#121826")
        sub_btn_frame.pack(fill="x")

        btn_save = tk.Button(sub_btn_frame, text="💾 Salvar Configurações", font=("Segoe UI", 9),
                             bg="#334155", fg="#F8FAFC", bd=0, padx=10, pady=5, cursor="hand2", command=self.apply_settings)
        btn_save.pack(side="left", expand=True, fill="x", padx=(0, 4))

        btn_pdf = tk.Button(sub_btn_frame, text="📄 Abrir Relatório PDF", font=("Segoe UI", 9),
                            bg="#334155", fg="#F8FAFC", bd=0, padx=10, pady=5, cursor="hand2", command=self.open_pdf)
        btn_pdf.pack(side="left", expand=True, fill="x", padx=(4, 0))

    def apply_settings(self):
        sel_res = self.res_var.get()
        if "3840" in sel_res:
            w, h = "3840", "2160"
        elif "2560" in sel_res:
            w, h = "2560", "1440"
        else:
            w, h = "1920", "1080"
        
        self.config["Graphics"]["width"] = w
        self.config["Graphics"]["height"] = h

        sel_fps = self.fps_var.get()
        if "120" in sel_fps:
            self.config["Graphics"]["fps_limit"] = "120"
        elif "Desbloqueado" in sel_fps:
            self.config["Graphics"]["fps_limit"] = "0"
            self.config["Graphics"]["vsync"] = "0"
        else:
            self.config["Graphics"]["fps_limit"] = "60"
            self.config["Graphics"]["vsync"] = "1"

        sel_pad = self.pad_var.get()
        if "Xbox" in sel_pad:
            self.config["Controls"]["pad_type"] = "xbox"
            self.config["Controls"]["trigger_state"] = "0"
        elif "Teclado" in sel_pad:
            self.config["Controls"]["pad_type"] = "keyboard"
        else:
            self.config["Controls"]["pad_type"] = "dualsense"
            self.config["Controls"]["trigger_state"] = "1"

        self.config["Controls"]["pad_motion"] = "1" if self.motion_var.get() else "0"
        self.config["Engine_GPU"]["shader_cache"] = "1" if self.cache_var.get() else "0"
        self.config["Graphics"]["perf_display"] = "1" if self.perf_var.get() else "0"

        save_config(self.config)
        self.write_launcher_bat()
        messagebox.showinfo("Configurações Salvas", "As configurações foram aplicadas e salvas com sucesso!")

    def write_launcher_bat(self):
        w = self.config.get("Graphics", "width", fallback="2560")
        h = self.config.get("Graphics", "height", fallback="1440")
        motion = self.config.get("Controls", "pad_motion", fallback="1")
        trig = self.config.get("Controls", "trigger_state", fallback="1")
        cache = self.config.get("Engine_GPU", "shader_cache", fallback="1")
        perf = self.config.get("Graphics", "perf_display", fallback="0")
        perf_flag = "true" if perf == "1" else "false"

        bat_content = f"""@echo off
setlocal
cd /d "{RUN_HYBRID_DIR}"

set APS5_GPU_ENGINE=kyty
set "KYTY_BARRIER_BATCH="
set "KYTY_DISPATCH_OVERLAP="
set "KYTY_WAVE64_WARPS="
set "ASTROHYBRID_COMMIT_EPOCH_FASTPATH=0"
set "KYTY_BUFFER_RANGE_FIXES=1"
set KYTY_HANG_WATCHDOG=0
set ASTROHYBRID_CACHE_TITLE_ID=PPSA21567
set KYTY_GPU_HOST_MODULE=astrohybrid_host.dll
set APS5_PAD_TRIGGER_STATE={trig}
set APS5_SAVEDATA_TRANSFER_FALLBACK=1
set ASTROHYBRID_PAD_MOTION={motion}
set ASTROHYBRID_SHADER_CACHE={cache}
set KYTY_CP_PRIORITY=2
set KYTY_DRAW_PREP_PRIORITY=2
set KYTY_DRAW_PREP_SPIN_US=10
set KYTY_DRAW_PREP_COLD_SPIN_US=5
set KYTY_GPU_WIDTH={w}
set KYTY_GPU_HEIGHT={h}

if exist "u59-preset.cmd" call "u59-preset.cmd"

echo ========================================================
echo   Iniciando Astro Bot PC ({w}x{h})
echo   GPU: AMD Radeon RX 9060 XT (16 GB VRAM)
echo   Controle: DualSense / XInput Motion={motion}
echo ========================================================
AstroBot.exe -perfDisplay {perf_flag} %*
"""
        target_bat = os.path.join(HYBRID_DIR, "Launch.bat")
        with open(target_bat, "w", encoding="utf-8") as f:
            f.write(bat_content)

    def launch_game(self):
        self.apply_settings()
        target_bat = os.path.join(HYBRID_DIR, "Launch.bat")
        subprocess.Popen(["cmd.exe", "/c", target_bat], cwd=RUN_HYBRID_DIR)
        self.destroy()

    def open_pdf(self):
        if os.path.exists(PDF_REPORT):
            os.startfile(PDF_REPORT)
        else:
            messagebox.showwarning("Arquivo não encontrado", "O relatório PDF não foi encontrado no local esperado.")

if __name__ == "__main__":
    app = AstroLauncherApp()
    app.mainloop()
