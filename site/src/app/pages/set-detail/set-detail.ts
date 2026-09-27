import { Component, OnInit, inject, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { ActivatedRoute, Router, RouterLink } from '@angular/router';
import { FormsModule } from '@angular/forms';
import { ApiService, WorkshopItem } from '../../core/services/api.service.js';
import { AuthService } from '../../core/services/auth.service.js';

@Component({
  selector: 'app-set-detail',
  standalone: true,
  imports: [CommonModule, RouterLink, FormsModule],
  templateUrl: './set-detail.html',
  styleUrl: './set-detail.scss'
})
export class SetDetailComponent implements OnInit {
  api = inject(ApiService);
  auth = inject(AuthService);
  private route = inject(ActivatedRoute);
  private router = inject(Router);

  item = signal<WorkshopItem | null>(null);
  saves = signal<WorkshopItem[]>([]);
  stamps = signal<WorkshopItem[]>([]);
  activeTab = signal<'saves' | 'stamps'>('saves');
  loading = signal<boolean>(true);

  editingItem = signal<boolean>(false);
  editTitle = signal<string>('');
  editDescription = signal<string>('');
  editThumbnailData = signal<string>('');
  editLoading = signal<boolean>(false);
  editError = signal<string>('');

  ngOnInit() {
    this.route.params.subscribe((params) => {
      const id = params['id'];
      if (id) {
        this.loadSet(id);
      }
    });
  }

  loadSet(id: string) {
    this.loading.set(true);
    this.api.getItem(id).subscribe({
      next: (set) => {
        this.item.set(set);
        this.loading.set(false);
      },
      error: (e) => {
        console.error('Failed to load set', e);
        this.loading.set(false);
      }
    });

    this.api.getSetSaves(id).subscribe({
      next: (savesList) => this.saves.set(savesList),
      error: (e) => console.error('Failed to load set saves', e)
    });

    this.api.getSetStamps(id).subscribe({
      next: (stampsList) => this.stamps.set(stampsList),
      error: (e) => console.error('Failed to load set stamps', e)
    });
  }

  isOwner(): boolean {
    const it = this.item();
    const user = this.auth.currentUser();
    if (!it || !user) return false;
    if (it.user_id && it.user_id === user.id) return true;
    if (!it.user_id && it.author.toLowerCase() === user.username.toLowerCase()) return true;
    return false;
  }

  toggleLike() {
    const it = this.item();
    if (!it) return;
    this.api.toggleLike(it.id).subscribe((res) => {
      this.item.update((val) => (val ? { ...val, is_liked: res.is_liked, likes_count: res.likes_count } : null));
    });
  }

  toggleFavorite() {
    const it = this.item();
    if (!it) return;
    this.api.toggleFavorite(it.id).subscribe((res) => {
      this.item.update((val) => (val ? { ...val, is_favorited: res.is_favorited, favorites_count: res.favorites_count } : null));
    });
  }

  openEditModal() {
    const it = this.item();
    if (!it) return;
    this.editTitle.set(it.title);
    this.editDescription.set(it.description);
    this.editThumbnailData.set('');
    this.editError.set('');
    this.editingItem.set(true);
  }

  closeEditModal() {
    this.editingItem.set(false);
  }

  onThumbnailSelected(event: Event) {
    const input = event.target as HTMLInputElement;
    if (input.files && input.files[0]) {
      const reader = new FileReader();
      reader.onload = (e: any) => {
        this.editThumbnailData.set(e.target.result);
      };
      reader.readAsDataURL(input.files[0]);
    }
  }

  submitEdit() {
    const it = this.item();
    if (!it) return;

    this.editLoading.set(true);
    this.editError.set('');

    const payload: any = {
      title: this.editTitle().trim(),
      description: this.editDescription().trim()
    };
    if (this.editThumbnailData()) {
      payload.thumbnail_data = this.editThumbnailData();
    }

    this.api.updateItem(it.id, payload).subscribe({
      next: (updated) => {
        this.editLoading.set(false);
        this.item.update((val) => (val ? { ...val, ...updated } : null));
        this.closeEditModal();
      },
      error: (err) => {
        this.editLoading.set(false);
        this.editError.set(err.error?.error || 'Failed to update set.');
      }
    });
  }

  deleteSet() {
    const it = this.item();
    if (!it) return;
    if (!confirm(`Are you sure you want to delete "${it.title}"? All attached saves and stamps will become orphaned.`)) {
      return;
    }

    this.api.deleteItem(it.id).subscribe({
      next: () => {
        this.router.navigate(['/workshop']);
      },
      error: (err) => {
        alert(err.error?.error || 'Failed to delete set.');
      }
    });
  }

  showUsageModal = signal<boolean>(false);
  currentOpenItem = signal<WorkshopItem | null>(null);

  getDownloadUrl(id: string): string {
    return this.api.getDownloadUrl(id);
  }

  openInSand3(it: WorkshopItem) {
    this.currentOpenItem.set(it);
    const uri = `sand3://open?type=${it.type}&id=${it.id}`;
    let appOpened = false;

    const onBlur = () => {
      appOpened = true;
    };
    window.addEventListener('blur', onBlur, { once: true });

    window.location.href = uri;

    setTimeout(() => {
      window.removeEventListener('blur', onBlur);
      if (!appOpened) {
        const dlLink = document.createElement('a');
        dlLink.href = this.getDownloadUrl(it.id);
        dlLink.download = '';
        document.body.appendChild(dlLink);
        dlLink.click();
        document.body.removeChild(dlLink);

        this.showUsageModal.set(true);
      }
    }, 1200);
  }

  closeUsageModal() {
    this.showUsageModal.set(false);
  }

  parseMeta(metaJson?: string): any {
    if (!metaJson) return {};
    try {
      return JSON.parse(metaJson);
    } catch {
      return {};
    }
  }

  getDimensions(subItem: WorkshopItem): string | null {
    const meta = this.parseMeta(subItem.meta_json);
    if (meta.width && meta.height) {
      return `${meta.width}×${meta.height}`;
    }
    return null;
  }
}
