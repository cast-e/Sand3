import { Component, OnInit, inject, signal } from '@angular/core';
import { CommonModule } from '@angular/common';
import { Router, ActivatedRoute, RouterLink } from '@angular/router';
import { FormsModule } from '@angular/forms';
import { ApiService, WorkshopItem } from '../../core/services/api.service.js';
import { AuthService } from '../../core/services/auth.service.js';

@Component({
  selector: 'app-upload',
  standalone: true,
  imports: [CommonModule, FormsModule, RouterLink],
  templateUrl: './upload.html',
  styleUrl: './upload.scss'
})
export class UploadComponent implements OnInit {
  api = inject(ApiService);
  auth = inject(AuthService);
  private router = inject(Router);
  private route = inject(ActivatedRoute);

  type = signal<'set' | 'save' | 'stamp' | 'theme'>('save');
  title = signal<string>('');
  description = signal<string>('');
  author = signal<string>('');
  parentSetId = signal<string>('');
  setHash = signal<string>('');

  selectedFile: File | null = null;
  thumbnailData = signal<string>('');

  availableSets = signal<WorkshopItem[]>([]);
  publishing = signal<boolean>(false);
  errorMessage = signal<string>('');
  successMessage = signal<string>('');

  ngOnInit() {
    const user = this.auth.currentUser();
    if (user) {
      this.author.set(user.username);
    } else {
      const savedAuthor = localStorage.getItem('sand3_author_name') || '';
      if (savedAuthor) this.author.set(savedAuthor);
    }

    this.route.queryParams.subscribe((params) => {
      if (params['parent_set_id']) {
        this.parentSetId.set(params['parent_set_id']);
      }
      if (params['type']) {
        const t = params['type'];
        if (t === 'set' || t === 'save' || t === 'stamp' || t === 'theme') {
          this.type.set(t);
        }
      }
    });

    this.api.getItems('set', 'popular', '', undefined, 1).subscribe({
      next: (res) => this.availableSets.set(res.items),
      error: () => { }
    });
  }

  onFileSelected(event: any) {
    const file = event.target.files?.[0];
    if (file) {
      this.selectedFile = file;
      if (!this.title()) {
        const baseName = file.name.replace(/\.[^/.]+$/, '');
        this.title.set(baseName.replace(/[_-]/g, ' '));
      }
    }
  }

  onThumbnailSelected(event: any) {
    const file = event.target.files?.[0];
    if (file) {
      const reader = new FileReader();
      reader.onload = (e: any) => {
        this.thumbnailData.set(e.target.result);
      };
      reader.readAsDataURL(file);
    }
  }

  onParentSetChange(setId: string) {
    this.parentSetId.set(setId);
    const selected = this.availableSets().find((s) => s.id === setId);
    if (selected && selected.set_hash) {
      this.setHash.set(selected.set_hash);
    } else {
      this.setHash.set('');
    }
  }

  submit() {
    this.errorMessage.set('');
    this.successMessage.set('');

    if (!this.title().trim()) {
      this.errorMessage.set('Please provide a title for your item.');
      return;
    }
    if (!this.author().trim()) {
      this.errorMessage.set('Please provide an author name.');
      return;
    }
    if (!this.selectedFile) {
      this.errorMessage.set('Please select a file to upload.');
      return;
    }

    localStorage.setItem('sand3_author_name', this.author().trim());

    this.publishing.set(true);

    const formData = new FormData();
    formData.append('type', this.type());
    formData.append('title', this.title().trim());
    formData.append('description', this.description().trim());
    formData.append('author', this.author().trim());

    if ((this.type() === 'save' || this.type() === 'stamp') && this.parentSetId()) {
      formData.append('parent_set_id', this.parentSetId());
      if (this.setHash()) {
        formData.append('set_hash', this.setHash());
      }
    }

    if (this.thumbnailData()) {
      formData.append('thumbnail_data', this.thumbnailData());
    }

    formData.append('file', this.selectedFile);

    this.api.publishItem(formData).subscribe({
      next: (created) => {
        this.publishing.set(false);
        this.successMessage.set('Published successfully! Redirecting...');
        setTimeout(() => {
          if (created.type === 'set') {
            this.router.navigate(['/workshop/sets', created.id]);
          } else {
            this.router.navigate(['/workshop']);
          }
        }, 1200);
      },
      error: (err) => {
        this.publishing.set(false);
        this.errorMessage.set(err.error?.error || 'Failed to upload item. Please try again.');
      }
    });
  }
}
