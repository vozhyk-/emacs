;;;

;;; Code:
(eval-when-compile (require 'cl-lib))
(or (featurep 'pgtk)
    (error "%s: Loading pgtk-win.el but not compiled for pure Gtk+-3."
           (invocation-name)))

;; Documentation-purposes only: actually loaded in loadup.el.
(require 'term/common-win)
(require 'frame)
(require 'mouse)
(require 'scroll-bar)
(require 'faces)
(require 'menu-bar)
(require 'fontset)
(require 'dnd)

(defgroup pgtk nil
  "GNUstep/macOS specific features."
  :group 'environment)

;;;; Command line argument handling.

(defvar x-invocation-args)
;; Set in term/common-win.el; currently unused by Gtk's x-open-connection.
(defvar x-command-line-resources)

;; pgtkterm.c.
(defvar pgtk-input-file)

(defun pgtk-handle-nxopen (_switch &optional temp)
  (setq unread-command-events (append unread-command-events
                                      (if temp '(pgtk-open-temp-file)
                                        '(pgtk-open-file)))
        pgtk-input-file (append pgtk-input-file (list (pop x-invocation-args)))))

(defun pgtk-handle-nxopentemp (switch)
  (pgtk-handle-nxopen switch t))

(defun pgtk-ignore-1-arg (_switch)
  (setq x-invocation-args (cdr x-invocation-args)))

(defun pgtk-parse-geometry (geom)
  "Parse a Nextstep-style geometry string GEOM.
Returns an alist of the form ((top . TOP), (left . LEFT) ... ).
The properties returned may include `top', `left', `height', and `width'."
  (when (string-match "\\([0-9]+\\)\\( \\([0-9]+\\)\\( \\([0-9]+\\)\
\\( \\([0-9]+\\) ?\\)?\\)?\\)?"
		      geom)
    (apply
     'append
     (list
      (list (cons 'top (string-to-number (match-string 1 geom))))
      (if (match-string 3 geom)
	  (list (cons 'left (string-to-number (match-string 3 geom)))))
      (if (match-string 5 geom)
	  (list (cons 'height (string-to-number (match-string 5 geom)))))
      (if (match-string 7 geom)
	  (list (cons 'width (string-to-number (match-string 7 geom)))))))))

;;;; Keyboard mapping.

(define-obsolete-variable-alias 'pgtk-alternatives-map 'x-alternatives-map "24.1")

;; Here are some Nextstep-like bindings for command key sequences.
(define-key global-map [?\s-,] 'customize)
(define-key global-map [?\s-'] 'next-multiframe-window)
(define-key global-map [?\s-`] 'other-frame)
(define-key global-map [?\s-~] 'pgtk-prev-frame)
(define-key global-map [?\s--] 'center-line)
(define-key global-map [?\s-:] 'ispell)
(define-key global-map [?\s-?] 'info)
(define-key global-map [?\s-^] 'kill-some-buffers)
(define-key global-map [?\s-&] 'kill-current-buffer)
(define-key global-map [?\s-C] 'pgtk-popup-color-panel)
(define-key global-map [?\s-D] 'dired)
(define-key global-map [?\s-E] 'edit-abbrevs)
(define-key global-map [?\s-L] 'shell-command)
(define-key global-map [?\s-M] 'manual-entry)
(define-key global-map [?\s-S] 'pgtk-write-file-using-panel)
(define-key global-map [?\s-a] 'mark-whole-buffer)
(define-key global-map [?\s-c] 'pgtk-copy-including-secondary)
(define-key global-map [?\s-d] 'isearch-repeat-backward)
(define-key global-map [?\s-e] 'isearch-yank-kill)
(define-key global-map [?\s-f] 'isearch-forward)
(define-key global-map [?\s-g] 'isearch-repeat-forward)
(define-key global-map [?\s-h] 'pgtk-do-hide-emacs)
(define-key global-map [?\s-H] 'pgtk-do-hide-others)
(define-key global-map [?\M-\s-h] 'pgtk-do-hide-others)
(define-key key-translation-map [?\M-\s-\u02D9] [?\M-\s-h])
(define-key global-map [?\s-j] 'exchange-point-and-mark)
(define-key global-map [?\s-k] 'kill-current-buffer)
(define-key global-map [?\s-l] 'goto-line)
(define-key global-map [?\s-m] 'iconify-frame)
(define-key global-map [?\s-n] 'make-frame)
(define-key global-map [?\s-o] 'pgtk-open-file-using-panel)
(define-key global-map [?\s-p] 'pgtk-print-buffer)
(define-key global-map [?\s-q] 'save-buffers-kill-emacs)
(define-key global-map [?\s-s] 'save-buffer)
(define-key global-map [?\s-t] 'pgtk-popup-font-panel)
(define-key global-map [?\s-u] 'revert-buffer)
(define-key global-map [?\s-v] 'yank)
(define-key global-map [?\s-w] 'delete-frame)
(define-key global-map [?\s-x] 'kill-region)
(define-key global-map [?\s-y] 'pgtk-paste-secondary)
(define-key global-map [?\s-z] 'undo)
(define-key global-map [?\s-|] 'shell-command-on-region)
(define-key global-map [s-kp-bar] 'shell-command-on-region)
;; (as in Terminal.app)
(define-key global-map [s-right] 'pgtk-next-frame)
(define-key global-map [s-left] 'pgtk-prev-frame)

(define-key global-map [home] 'beginning-of-buffer)
(define-key global-map [end] 'end-of-buffer)
(define-key global-map [kp-home] 'beginning-of-buffer)
(define-key global-map [kp-end] 'end-of-buffer)
(define-key global-map [kp-prior] 'scroll-down-command)
(define-key global-map [kp-next] 'scroll-up-command)

;; Allow shift-clicks to work similarly to under Nextstep.
(define-key global-map [S-mouse-1] 'mouse-save-then-kill)
(global-unset-key [S-down-mouse-1])

;; Special Nextstep-generated events are converted to function keys.  Here
;; are the bindings for them.  Note, these keys are actually declared in
;; x-setup-function-keys in common-win.
(define-key global-map [pgtk-power-off] 'save-buffers-kill-emacs)
(define-key global-map [pgtk-open-file] 'pgtk-find-file)
(define-key global-map [pgtk-open-temp-file] [pgtk-open-file])
(define-key global-map [pgtk-change-font] 'pgtk-respond-to-change-font)
(define-key global-map [pgtk-open-file-line] 'pgtk-open-file-select-line)
(define-key global-map [pgtk-new-frame] 'make-frame)
(define-key global-map [pgtk-toggle-toolbar] 'pgtk-toggle-toolbar)
(define-key global-map [pgtk-show-prefs] 'customize)


;; pgtkterm.c
(defvar pgtk-input-spi-name)
(defvar pgtk-input-spi-arg)

(declare-function dnd-open-file "dnd" (uri action))

;; Handles multiline strings that are passed to the "open-file" service.
(defun pgtk-open-file-service (filenames)
  "Open multiple files when selecting a multiline string FILENAMES."
  (let ((filelist (split-string filenames "[\n\r]+" t "[ \u00A0\t]+")))
    ;; The path strings are trimmed for spaces, nbsp and tabs.
    (dolist (filestring filelist)
      (dnd-open-file filestring nil))))


;; Composed key sequence handling for Nextstep system input methods.
;; (On Nextstep systems, input methods are provided for CJK
;; characters, etc. which require multiple keystrokes, and during
;; entry a partial ("working") result is typically shown in the
;; editing window.)

(defface pgtk-working-text-face
  '((t :underline t))
  "Face used to highlight working text during compose sequence insert."
  :group 'pgtk)

(defvar pgtk-working-overlay nil
  "Overlay used to highlight working text during compose sequence insert.
When text is in th echo area, this just stores the length of the working text.")

(defvar pgtk-working-text)		; pgtkterm.c

;; Test if in echo area, based on mac-win.el 2007/08/26 unicode-2.
;; This will fail if called from a NONASCII_KEYSTROKE event on the global map.
(defun pgtk-in-echo-area ()
  "Whether, for purposes of inserting working composition text, the minibuffer
is currently being used."
  (or isearch-mode
      (and cursor-in-echo-area (current-message))
      ;; Overlay strings are not shown in some cases.
      (get-char-property (point) 'invisible)
      (and (not (bobp))
	   (or (and (get-char-property (point) 'display)
		    (eq (get-char-property (1- (point)) 'display)
			(get-char-property (point) 'display)))
	       (and (get-char-property (point) 'composition)
		    (eq (get-char-property (1- (point)) 'composition)
			(get-char-property (point) 'composition)))))))

;; The 'interactive' here stays for subinvocations, so the pgtk-in-echo-area
;; always returns nil for some reason.  If this WASN'T the case, we could
;; map this to [pgtk-insert-working-text] and eliminate Fevals in pgtkterm.c.
;; These functions test whether in echo area and delegate accordingly.
(defun pgtk-put-working-text ()
  (interactive)
  (if (pgtk-in-echo-area) (pgtk-echo-working-text) (pgtk-insert-working-text)))
(defun pgtk-unput-working-text ()
  (interactive)
  (pgtk-delete-working-text))

(defun pgtk-insert-working-text ()
  "Insert contents of `pgtk-working-text' as UTF-8 string and mark with
`pgtk-working-overlay'.  Any previously existing working text is cleared first.
The overlay is assigned the face `pgtk-working-text-face'."
  ;; FIXME: if buffer is read-only, don't try to insert anything
  ;;  and if text is bound to a command, execute that instead (Bug#1453)
  (interactive)
  (pgtk-delete-working-text)
  (let ((start (point)))
    (insert pgtk-working-text)
    (overlay-put (setq pgtk-working-overlay (make-overlay start (point)
							(current-buffer) nil t))
		 'face 'pgtk-working-text-face)))

(defun pgtk-echo-working-text ()
  "Echo contents of `pgtk-working-text' in message display area.
See `pgtk-insert-working-text'."
  (pgtk-delete-working-text)
  (let* ((msg (current-message))
	 (msglen (length msg))
	 message-log-max)
    (setq pgtk-working-overlay (length pgtk-working-text))
    (setq msg (concat msg pgtk-working-text))
    (put-text-property msglen (+ msglen pgtk-working-overlay)
		       'face 'pgtk-working-text-face msg)
    (message "%s" msg)))

(defun pgtk-delete-working-text()
  "Delete working text and clear `pgtk-working-overlay'."
  (interactive)
  (cond
   ((and (overlayp pgtk-working-overlay)
         ;; Still alive?
         (overlay-buffer pgtk-working-overlay))
    (with-current-buffer (overlay-buffer pgtk-working-overlay)
      (delete-region (overlay-start pgtk-working-overlay)
                     (overlay-end pgtk-working-overlay))
      (delete-overlay pgtk-working-overlay)))
   ((integerp pgtk-working-overlay)
    (let ((msg (current-message))
          message-log-max)
      (setq msg (substring msg 0 (- (length msg) pgtk-working-overlay)))
      (message "%s" msg))))
  (setq pgtk-working-overlay nil))


;; macOS file system Unicode UTF-8 NFD (decomposed form) support.
(when (eq system-type 'darwin)
  ;; Used prior to Emacs 25.
  (define-coding-system-alias 'utf-8-nfd 'utf-8-hfs)

  (set-file-name-coding-system 'utf-8-hfs))

;;;; Inter-app communications support.

(defun pgtk-insert-file ()
  "Insert contents of file `pgtk-input-file' like insert-file but with less
prompting.  If file is a directory perform a `find-file' on it."
  (interactive)
  (let ((f (pop pgtk-input-file)))
    (if (file-directory-p f)
        (find-file f)
      (push-mark (+ (point) (cadr (insert-file-contents f)))))))

(defvar pgtk-select-overlay nil
  "Overlay used to highlight areas in files requested by Nextstep apps.")
(make-variable-buffer-local 'pgtk-select-overlay)

(defvar pgtk-input-line) 			; pgtkterm.c

(defun pgtk-open-file-select-line ()
  "Open a buffer containing the file `pgtk-input-file'.
Lines are highlighted according to `pgtk-input-line'."
  (interactive)
  (pgtk-find-file)
  (cond
   ((and pgtk-input-line (buffer-modified-p))
    (if pgtk-select-overlay
        (setq pgtk-select-overlay (delete-overlay pgtk-select-overlay)))
    (deactivate-mark)
    (goto-char (point-min))
    (forward-line (1- (if (consp pgtk-input-line)
                          (min (car pgtk-input-line) (cdr pgtk-input-line))
                        pgtk-input-line))))
   (pgtk-input-line
    (if (not pgtk-select-overlay)
        (overlay-put (setq pgtk-select-overlay (make-overlay (point-min)
                                                           (point-min)))
                     'face 'highlight))
    (let ((beg (save-excursion
                 (goto-char (point-min))
                 (line-beginning-position
                  (if (consp pgtk-input-line)
                      (min (car pgtk-input-line) (cdr pgtk-input-line))
                    pgtk-input-line))))
          (end (save-excursion
                 (goto-char (point-min))
                 (line-beginning-position
                  (1+ (if (consp pgtk-input-line)
                          (max (car pgtk-input-line) (cdr pgtk-input-line))
                        pgtk-input-line))))))
      (move-overlay pgtk-select-overlay beg end)
      (deactivate-mark)
      (goto-char beg)))
   (t
    (if pgtk-select-overlay
        (setq pgtk-select-overlay (delete-overlay pgtk-select-overlay))))))

(defun pgtk-unselect-line ()
  "Removes any Nextstep highlight a buffer may contain."
  (if pgtk-select-overlay
      (setq pgtk-select-overlay (delete-overlay pgtk-select-overlay))))

(add-hook 'first-change-hook 'pgtk-unselect-line)

;;;; Preferences handling.
(declare-function pgtk-get-resource "pgtkfns.c" (owner name))

(defun get-lisp-resource (arg1 arg2)
  (let ((res (pgtk-get-resource arg1 arg2)))
    (cond
     ((not res) 'unbound)
     ((string-equal (upcase res) "YES") t)
     ((string-equal (upcase res) "NO")  nil)
     (t (read res)))))

;; pgtkterm.c

(declare-function pgtk-read-file-name "pgtkfns.c"
		  (prompt &optional dir mustmatch init dir_only_p))

;;;; File handling.

(defun x-file-dialog (prompt dir default_filename mustmatch only_dir_p)
"Read file name, prompting with PROMPT in directory DIR.
Use a file selection dialog.  Select DEFAULT-FILENAME in the dialog's file
selection box, if specified.  If MUSTMATCH is non-nil, the returned file
or directory must exist.

This function is only defined on PGTK, MS Windows, and X Windows with the
Motif or Gtk toolkits.  With the Motif toolkit, ONLY-DIR-P is ignored.
Otherwise, if ONLY-DIR-P is non-nil, the user can only select directories."
  (pgtk-read-file-name prompt dir mustmatch default_filename only_dir_p))

(defun pgtk-open-file-using-panel ()
  "Pop up open-file panel, and load the result in a buffer."
  (interactive)
  ;; Prompt dir defaultName isLoad initial.
  (setq pgtk-input-file (pgtk-read-file-name "Select File to Load" nil t nil))
  (if pgtk-input-file
      (and (setq pgtk-input-file (list pgtk-input-file)) (pgtk-find-file))))

(defun pgtk-write-file-using-panel ()
  "Pop up save-file panel, and save buffer in resulting name."
  (interactive)
  (let (pgtk-output-file)
    ;; Prompt dir defaultName isLoad initial.
    (setq pgtk-output-file (pgtk-read-file-name "Save As" nil nil nil))
    (message pgtk-output-file)
    (if pgtk-output-file (write-file pgtk-output-file))))

(defcustom pgtk-pop-up-frames 'fresh
  "Non-nil means open files upon request from the Workspace in a new frame.
If t, always do so.  Any other non-nil value means open a new frame
unless the current buffer is a scratch buffer."
  :type '(choice (const :tag "Never" nil)
                 (const :tag "Always" t)
                 (other :tag "Except for scratch buffer" fresh))
  :version "23.1"
  :group 'pgtk)

(declare-function pgtk-hide-emacs "pgtkfns.c" (on))

(defun pgtk-find-file ()
  "Do a `find-file' with the `pgtk-input-file' as argument."
  (interactive)
  (let* ((f (file-truename
	     (expand-file-name (pop pgtk-input-file)
			       command-line-default-directory)))
         (file (find-file-noselect f))
         (bufwin1 (get-buffer-window file 'visible))
         (bufwin2 (get-buffer-window "*scratch*" 'visible)))
    (cond
     (bufwin1
      (select-frame (window-frame bufwin1))
      (raise-frame (window-frame bufwin1))
      (select-window bufwin1))
     ((and (eq pgtk-pop-up-frames 'fresh) bufwin2)
      (pgtk-hide-emacs 'activate)
      (select-frame (window-frame bufwin2))
      (raise-frame (window-frame bufwin2))
      (select-window bufwin2)
      (find-file f))
     (pgtk-pop-up-frames
      (pgtk-hide-emacs 'activate)
      (let ((pop-up-frames t)) (pop-to-buffer file nil)))
     (t
      (pgtk-hide-emacs 'activate)
      (find-file f)))))


(defun pgtk-drag-n-drop (event &optional new-frame force-text)
  "Edit the files listed in the drag-n-drop EVENT.
Switch to a buffer editing the last file dropped."
  (interactive "e")
  (let* ((window (posn-window (event-start event)))
         (arg (car (cdr (cdr event))))
         (type (car arg))
         (data (car (cdr arg)))
         (url-or-string (cond ((eq type 'file)
                               (concat "file:" data))
                              (t data))))
    (set-frame-selected-window nil window)
    (when new-frame
      (select-frame (make-frame)))
    (raise-frame)
    (setq window (selected-window))
    (if force-text
        (dnd-insert-text window 'private data)
      (dnd-handle-one-url window 'private url-or-string))))


(defun pgtk-drag-n-drop-other-frame (event)
  "Edit the files listed in the drag-n-drop EVENT, in other frames.
May create new frames, or reuse existing ones.  The frame editing
the last file dropped is selected."
  (interactive "e")
  (pgtk-drag-n-drop event t))

(defun pgtk-drag-n-drop-as-text (event)
  "Drop the data in EVENT as text."
  (interactive "e")
  (pgtk-drag-n-drop event nil t))

(defun pgtk-drag-n-drop-as-text-other-frame (event)
  "Drop the data in EVENT as text in a new frame."
  (interactive "e")
  (pgtk-drag-n-drop event t t))

(global-set-key [drag-n-drop] 'pgtk-drag-n-drop)
(global-set-key [C-drag-n-drop] 'pgtk-drag-n-drop-other-frame)
(global-set-key [M-drag-n-drop] 'pgtk-drag-n-drop-as-text)
(global-set-key [C-M-drag-n-drop] 'pgtk-drag-n-drop-as-text-other-frame)

;;;; Frame-related functions.

;; pgtkterm.c
(defvar pgtk-alternate-modifier)
(defvar pgtk-right-alternate-modifier)
(defvar pgtk-right-command-modifier)
(defvar pgtk-right-control-modifier)

;; You say tomAYto, I say tomAHto..
(defvaralias 'pgtk-option-modifier 'pgtk-alternate-modifier)
(defvaralias 'pgtk-right-option-modifier 'pgtk-right-alternate-modifier)

(defun pgtk-do-hide-emacs ()
  (interactive)
  (pgtk-hide-emacs t))

(declare-function pgtk-hide-others "pgtkfns.c" ())

(defun pgtk-do-hide-others ()
  (interactive)
  (pgtk-hide-others))

(declare-function pgtk-emacs-info-panel "pgtkfns.c" ())

(defun pgtk-do-emacs-info-panel ()
  (interactive)
  (pgtk-emacs-info-panel))

(defun pgtk-next-frame ()
  "Switch to next visible frame."
  (interactive)
  (other-frame 1))

(defun pgtk-prev-frame ()
  "Switch to previous visible frame."
  (interactive)
  (other-frame -1))

;; Frame will be focused anyway, so select it
;; (if this is not done, mode line is dimmed until first interaction)
;; FIXME: Sounds like we're working around a bug in the underlying code.
(add-hook 'after-make-frame-functions 'select-frame)

(defvar tool-bar-mode)
(declare-function tool-bar-mode "tool-bar" (&optional arg))

;; Based on a function by David Reitter <dreitter@inf.ed.ac.uk> ;
;; see https://lists.gnu.org/archive/html/emacs-devel/2005-09/msg00681.html .
(defun pgtk-toggle-toolbar (&optional frame)
  "Switches the tool bar on and off in frame FRAME.
 If FRAME is nil, the change applies to the selected frame."
  (interactive)
  (modify-frame-parameters
   frame (list (cons 'tool-bar-lines
		       (if (> (or (frame-parameter frame 'tool-bar-lines) 0) 0)
				   0 1)) ))
  (if (not tool-bar-mode) (tool-bar-mode t)))


;;;; Dialog-related functions.

;; Ask user for confirm before printing.  Due to Kevin Rodgers.
(defun pgtk-print-buffer ()
  "Interactive front-end to `print-buffer': asks for user confirmation first."
  (interactive)
  (if (and (called-interactively-p 'interactive)
           (or (listp last-nonmenu-event)
               (and (char-or-string-p (event-basic-type last-command-event))
                    (memq 'super (event-modifiers last-command-event)))))
      (let ((last-nonmenu-event (if (listp last-nonmenu-event)
                                    last-nonmenu-event
                                  ;; Fake it:
                                  `(mouse-1 POSITION 1))))
        (if (y-or-n-p (format "Print buffer %s? " (buffer-name)))
            (print-buffer)
	  (error "Canceled")))
    (print-buffer)))

;;;; Font support.

;; Needed for font listing functions under both backend and normal
(setq scalable-fonts-allowed t)

;; Set to use font panel instead
(declare-function pgtk-popup-font-panel "pgtkfns.c" (&optional frame))
(defalias 'x-select-font 'pgtk-popup-font-panel "Pop up the font panel.
This function has been overloaded in Nextstep.")
(defalias 'mouse-set-font 'pgtk-popup-font-panel "Pop up the font panel.
This function has been overloaded in Nextstep.")

;; pgtkterm.c
(defvar pgtk-input-font)
(defvar pgtk-input-fontsize)

(defun pgtk-respond-to-change-font ()
  "Respond to changeFont: event, expecting `pgtk-input-font' and\n\
`pgtk-input-fontsize' of new font."
  (interactive)
  (modify-frame-parameters (selected-frame)
                           (list (cons 'fontsize pgtk-input-fontsize)))
  (modify-frame-parameters (selected-frame)
                           (list (cons 'font pgtk-input-font)))
  (set-frame-font pgtk-input-font))


;; Default fontset for macOS.  This is mainly here to show how a fontset
;; can be set up manually.  Ordinarily, fontsets are auto-created whenever
;; a font is chosen by
(defvar pgtk-standard-fontset-spec
  ;; Only some code supports this so far, so use uglier XLFD version
  ;; "-pgtk-*-*-*-*-*-10-*-*-*-*-*-fontset-standard,latin:Courier,han:Kai"
  (mapconcat 'identity
             '("-*-Monospace-*-*-*-*-10-*-*-*-*-*-fontset-standard"
               "latin:-*-Courier-*-*-*-*-10-*-*-*-*-*-iso10646-1"
               "han:-*-Kai-*-*-*-*-10-*-*-*-*-*-iso10646-1"
               "cyrillic:-*-Trebuchet$MS-*-*-*-*-10-*-*-*-*-*-iso10646-1")
             ",")
  "String of fontset spec of the standard fontset.
This defines a fontset consisting of the Courier and other fonts that
come with macOS.
See the documentation of `create-fontset-from-fontset-spec' for the format.")

(defvar pgtk-reg-to-script)               ; pgtkfont.c

;; This maps font registries (not exposed by PGTK APIs for font selection) to
;; Unicode scripts (which can be mapped to Unicode character ranges which are).
;; See ../international/fontset.el
(setq pgtk-reg-to-script
      '(("iso8859-1" . latin)
	("iso8859-2" . latin)
	("iso8859-3" . latin)
	("iso8859-4" . latin)
	("iso8859-5" . cyrillic)
	("microsoft-cp1251" . cyrillic)
	("koi8-r" . cyrillic)
	("iso8859-6" . arabic)
	("iso8859-7" . greek)
	("iso8859-8" . hebrew)
	("iso8859-9" . latin)
	("iso8859-10" . latin)
	("iso8859-11" . thai)
	("tis620" . thai)
	("iso8859-13" . latin)
	("iso8859-14" . latin)
	("iso8859-15" . latin)
	("iso8859-16" . latin)
	("viscii1.1-1" . latin)
	("jisx0201" . kana)
	("jisx0208" . han)
	("jisx0212" . han)
	("jisx0213" . han)
	("gb2312.1980" . han)
	("gb18030" . han)
	("gbk-0" . han)
	("big5" . han)
	("cns11643" . han)
	("sisheng_cwnn" . bopomofo)
	("ksc5601.1987" . hangul)
	("ethiopic-unicode" . ethiopic)
	("is13194-devanagari" . indian-is13194)
	("iso10646.indian-1" . devanagari)))


;;;; Pasteboard support.

(define-obsolete-function-alias 'pgtk-store-cut-buffer-internal
  'gui-set-selection "24.1")


(defun pgtk-copy-including-secondary ()
  (interactive)
  (call-interactively 'kill-ring-save)
  (gui-set-selection 'SECONDARY (buffer-substring (point) (mark t))))

(defun pgtk-paste-secondary ()
  (interactive)
  (insert (gui-get-selection 'SECONDARY)))


;;;; Color support.

;; Functions for color panel + drag
(defun pgtk-face-at-pos (pos)
  (let* ((frame (car pos))
         (frame-pos (cons (cadr pos) (cddr pos)))
         (window (window-at (car frame-pos) (cdr frame-pos) frame))
         (window-pos (coordinates-in-window-p frame-pos window))
         (buffer (window-buffer window))
         (edges (window-edges window)))
    (cond
     ((not window-pos)
      nil)
     ((eq window-pos 'mode-line)
      'mode-line)
     ((eq window-pos 'vertical-line)
      'default)
     ((consp window-pos)
      (with-current-buffer buffer
        (let ((p (car (compute-motion (window-start window)
                                      (cons (nth 0 edges) (nth 1 edges))
                                      (window-end window)
                                      frame-pos
                                      (- (window-width window) 1)
                                      nil
                                      window))))
          (cond
           ((eq p (window-point window))
            'cursor)
           ((and mark-active (< (region-beginning) p) (< p (region-end)))
            'region)
           (t
	    (let ((faces (get-char-property p 'face window)))
	      (if (consp faces) (car faces) faces)))))))
     (t
      nil))))

(defun pgtk-suspend-error ()
  ;; Don't allow suspending if any of the frames are PGTK frames.
  (if (memq 'pgtk (mapcar 'window-system (frame-list)))
      (error "Cannot suspend Emacs while an PGTK GUI frame exists")))


;; Set some options to be as Nextstep-like as possible.
(setq frame-title-format t
      icon-title-format t)


(defvar pgtk-initialized nil
  "Non-nil if Nextstep windowing has been initialized.")

(declare-function x-handle-args "common-win" (args))
(declare-function x-open-connection "pgtkfns.c"
                  (display &optional xrm-string must-succeed))
(declare-function pgtk-set-resource "pgtkfns.c" (owner name value))

;; Do the actual Nextstep Windows setup here; the above code just
;; defines functions and variables that we use now.
(cl-defmethod window-system-initialization (&context (window-system pgtk)
                                            &optional _display)
  "Initialize Emacs for Nextstep (Cocoa / GNUstep) windowing."
  (cl-assert (not pgtk-initialized))

  ;; PENDING: not needed?
  (setq command-line-args (x-handle-args command-line-args))

  ;; Make sure we have a valid resource name.
  (or (stringp x-resource-name)
      (let (i)
	(setq x-resource-name (invocation-name))

	;; Change any . or * characters in x-resource-name to hyphens,
	;; so as not to choke when we use it in X resource queries.
	(while (setq i (string-match "[.*]" x-resource-name))
	  (aset x-resource-name i ?-))))

  ;; Setup the default fontset.
  (create-default-fontset)
  ;; Create the standard fontset.
  (condition-case err
      (create-fontset-from-fontset-spec pgtk-standard-fontset-spec t)
    (error (display-warning
            'initialization
            (format "Creation of the standard fontset failed: %s" err)
            :error)))

  ; (x-open-connection (system-name) x-command-line-resources t)
  (x-open-connection (or (getenv "WAYLAND_DISPLAY") (getenv "DISPLAY")) x-command-line-resources t)

  ;; FIXME: This will surely lead to "MODIFIED OUTSIDE CUSTOM" warnings.
  (menu-bar-mode (if (get-lisp-resource nil "Menus") 1 -1))

  ;; Mac OS X Lion introduces PressAndHold, which is unsupported by this port.
  ;; See this thread for more details:
  ;; https://lists.gnu.org/archive/html/emacs-devel/2011-06/msg00505.html
  (pgtk-set-resource nil "ApplePressAndHoldEnabled" "NO")

  (x-apply-session-resources)

  ;; Don't let Emacs suspend under PGTK.
  (add-hook 'suspend-hook 'pgtk-suspend-error)

  (setq pgtk-initialized t))

;; Any display name is OK.
(add-to-list 'display-format-alist '(".*" . pgtk))
(cl-defmethod handle-args-function (args &context (window-system pgtk))
  (x-handle-args args))

(cl-defmethod frame-creation-function (params &context (window-system pgtk))
  (x-create-frame-with-faces params))

(declare-function pgtk-own-selection-internal "pgtkselect.c" (selection value &optional frame))
(declare-function pgtk-disown-selection-internal "pgtkselect.c" (selection &optional time_object terminal))
(declare-function pgtk-selection-owner-p "pgtkselect.c" (&optional selection terminal))
(declare-function pgtk-selection-exists-p "pgtkselect.c" (&optional selection terminal))
(declare-function pgtk-get-selection-internal "pgtkselect.c" (selection-symbol target-type &optional time_stamp terminal))

(cl-defmethod gui-backend-set-selection (selection value
                                         &context (window-system pgtk))
  (if value (pgtk-own-selection-internal selection value)
    (pgtk-disown-selection-internal selection)))

(cl-defmethod gui-backend-selection-owner-p (selection
                                             &context (window-system pgtk))
  (pgtk-selection-owner-p selection))

(cl-defmethod gui-backend-selection-exists-p (selection
                                              &context (window-system pgtk))
  (pgtk-selection-exists-p selection))

(cl-defmethod gui-backend-get-selection (selection-symbol target-type
                                         &context (window-system pgtk))
  (pgtk-get-selection-internal selection-symbol target-type))

;; If you want to use alt key as alt key:
;;   (setq x-alt-keysym nil)
(setq x-alt-keysym 'meta)

(provide 'pgtk-win)
(provide 'term/pgtk-win)

;;; pgtk-win.el ends here
